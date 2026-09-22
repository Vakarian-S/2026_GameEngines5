#include "utils/MemoryMonitor.h"

#include <atomic>
/* std::bit_ceil */
#include <bit>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <format>
/* _aligned_malloc / _aligned_free */
#include <malloc.h>
#include <optional>
/* std::make_signed_t */
#include <type_traits>


#if MEMMON_CHECKED_HEADER
/* SRWLOCK for the leak list. The defines keep windows.h small and stop its min and max macros. */
/* Lean and Mean just tells c++ to exclude useless some libraries for faster performance */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif


namespace
{
    /* ============================== Counters ============================== */

    /**
     * The live counters. Each field is its own atomic, so threads can add to it at the same time
     * without losing a count. GetStats() copies them into a plain MemoryStats.
     */
    struct AtomicStats
    {
        /** Total calls to new */
        std::atomic<std::uint64_t> allocationCount{0};
        /** Total calls to delete that really freed */
        std::atomic<std::uint64_t> freeCount{0};
        /** Total bytes asked for */
        std::atomic<std::uint64_t> bytesAllocated{0};
        /** Total bytes given back */
        std::atomic<std::uint64_t> bytesFreed{0};

        /** Bytes alive right now. Its own counter, so each new and delete gets the exact value in one step. */
        std::atomic<std::size_t> liveBytes{0};

        /** Most memory alive at once */
        std::atomic<std::size_t> peakLiveBytes{0};
        /** Biggest single block */
        std::atomic<std::size_t> largestAllocation{0};

        /** Frees refused because the block's stamp was wrong */
        std::atomic<std::uint64_t> badBlockCount{0};
        /** Frees that came through the wrong delete */
        std::atomic<std::uint64_t> mismatchCount{0};
    };

    /* A counter that is not lock-free has a hidden lock inside. These fail the build if that ever happens. */
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free, "64-bit counters must be lock-free");
    static_assert(std::atomic<std::size_t>::is_always_lock_free, "size_t counters must be lock-free");

    /** constinit sets these before any code runs, so allocations made before main are safe to count */
    constinit AtomicStats stats{};

    /** On from the start, so the runtime's allocations before main are logged too */
    constinit std::atomic<bool> loggingEnabled{true};


    /* ============================== Block layout ============================== */

#if MEMMON_CHECKED_HEADER
    /** Stamped on every live block. Any other value means the pointer is not ours. */
    constexpr std::uint32_t LiveMagic = 0xC0FFEEu;

    /** Stamped on the way out, so a second delete of the same block can be caught */
    constexpr std::uint32_t FreedMagic = 0xDEAD5EEDu;

    /**
     * Bytes used by the Debug fields: 3 pointers, 3 size_t and 2 uint32.
     * 56 on x64, 32 on Win32.
     */
    constexpr std::size_t CheckedFieldBytes =
        3 * sizeof(void*) + 3 * sizeof(std::size_t) + 2 * sizeof(std::uint32_t);

    /**
     * Rounded up to a power of two: 64 on x64, 32 on Win32.
     * It MUST be a power of two, or _aligned_malloc kills the process.
     */
    constexpr std::size_t CheckedHeaderSize = std::bit_ceil(CheckedFieldBytes);

    /** Spare bytes, filled so that magic ends up last: 2 words on x64, 0 on Win32 */
    constexpr std::size_t HeaderPadBytes = CheckedHeaderSize - CheckedFieldBytes;
    static_assert(HeaderPadBytes % sizeof(std::uint32_t) == 0);
    constexpr std::size_t HeaderPadWords = HeaderPadBytes / sizeof(std::uint32_t);

    /** C++ does not allow an array of size 0, so Pad<0> is an empty type instead */
    template <std::size_t Words>
    struct Pad
    {
        std::uint32_t words[Words];
    };

    template <>
    struct Pad<0>
    {
    };
#endif

    /**
     * The label stored in front of every block. The caller never sees it.
     */
    struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) Header
    {
#if MEMMON_CHECKED_HEADER
        /** The live block before this one in the leak list */
        Header* prev;

        /** The live block after this one in the leak list */
        Header* next;

        /** Source file of the new, nullptr if unknown */
        const char* file;
#endif
        /** Bytes the caller asked for */
        std::size_t numBytes;

        /** 0 = malloc block, otherwise _aligned_malloc with this alignment */
        std::size_t alignment;
#if MEMMON_CHECKED_HEADER
        /** Allocation number, used to list only leaks after a baseline */
        std::size_t serial;

        /** Source line of the new, 0 if unknown */
        std::uint32_t line;

        /**
         * Filler, keeps magic last.
         *
         * [[msvc::no_unique_address]]: normally every field takes at least 1 byte, even an
         * empty one, so an empty Pad<0> would still cost 1 byte plus padding and push magic
         * off the end. This attribute lets an empty field take 0 bytes.
         * It is MSVC's spelling of the standard [[no_unique_address]], which MSVC ignores.
         */
        [[msvc::no_unique_address]] Pad<HeaderPadWords> reserved;

        /** LiveMagic or FreedMagic. Last, so writing just before the block hits it */
        std::uint32_t magic;
#endif
    };

#if MEMMON_CHECKED_HEADER
    /** First item in our list of allocated headers for the leak list */
    constinit Header* liveHead = nullptr;

    /** Guards liveHead and every prev and next. All zeros, so it works before main and never allocates. */
    constinit SRWLOCK listLock = SRWLOCK_INIT;
#endif

    /**
     * Holds the list lock from here to the end of the scope, so every way out releases it.
     * Nothing done while holding it may allocate: SRWLOCK is not recursive, so a new in there waits forever.
     * Empty in Release, where there is no list to guard.
     */
    struct ListGuard
    {
#if MEMMON_CHECKED_HEADER
        /* Acquires a slim reader/writer (SRW) lock in exclusive mode. */
        ListGuard() noexcept { AcquireSRWLockExclusive(&listLock); }
        /* Releases a slim reader/writer (SRW) lock that was acquired in exclusive mode.*/
        ~ListGuard() { ReleaseSRWLockExclusive(&listLock); }
#else
        ListGuard() noexcept
        {
        }
#endif
        /* A copy would release the lock twice */
        ListGuard(const ListGuard&) = delete;
        ListGuard& operator=(const ListGuard&) = delete;
    };


    static_assert(sizeof(Header) >= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    static_assert((sizeof(Header) & (sizeof(Header) - 1)) == 0, "Header size must be a power of two");
#if MEMMON_CHECKED_HEADER
    /* Fails if the fields change, or if the compiler adds hidden padding between them */
    static_assert(sizeof(Header) == CheckedHeaderSize, "Header fields no longer add up to CheckedHeaderSize");
    static_assert(offsetof(Header, magic) + sizeof(Header::magic) == sizeof(Header),
                  "magic must be the last bytes of the header, so a small underrun lands on it");
#endif

    /* ============================== Utility ============================== */

    enum class BlockCheck { Ok, NotOurs, AlreadyFreed };

    /**
     * Reads the block's stamp. A pointer to unmapped memory still crashes on this read.
     * @param header
     */
    BlockCheck CheckHeader([[maybe_unused]] const Header* header) noexcept
    {
#if MEMMON_CHECKED_HEADER
        if (header->magic == LiveMagic) return BlockCheck::Ok;
        if (header->magic == FreedMagic) return BlockCheck::AlreadyFreed;
        return BlockCheck::NotOurs;
#else
        return BlockCheck::Ok;
#endif
    }

    /**
     * Fills in a new header
     * @param header
     * @param size bytes the caller asked for
     * @param alignment 0 for a malloc block, the real value for an _aligned_malloc one
     * @param file source file of the new, nullptr if unknown
     * @param line source line of the new, 0 if unknown
     */
    void InitHeader(Header* header, const std::size_t size, const std::size_t alignment,
                    [[maybe_unused]] const char* file, [[maybe_unused]] const std::uint32_t line) noexcept
    {
        header->numBytes = size;
        header->alignment = alignment;
#if MEMMON_CHECKED_HEADER
        header->prev = nullptr;
        header->next = nullptr;
        header->file = file;
        header->serial = 0;
        header->line = line;
        header->magic = LiveMagic;
#endif
    }

    /**
     * Stamps the block as freed, so a second delete of it is caught
     * @param header
     */
    void MarkFreed([[maybe_unused]] Header* header) noexcept
    {
#if MEMMON_CHECKED_HEADER
        header->magic = FreedMagic;
#endif
    }

    /**
     * Bytes in front of an aligned block: the alignment, or the header size if that is bigger.
     * Both are powers of two, so the result is one too.
     * @param alignment
     */
    std::size_t HeaderSpace(const std::size_t alignment)
    {
        return alignment > sizeof(Header) ? alignment : sizeof(Header);
    }

    /**
     * Runs on every new block, puts the new linked allocation at the front of our leak list
     * Call with a ListGuard held.
     * @param header
     * @param serial the allocation number from CountAllocation, taken in the same atomic step
     */
    void LinkBlock([[maybe_unused]] Header* header, [[maybe_unused]] const std::uint64_t serial) noexcept
    {
#if MEMMON_CHECKED_HEADER
        header->next = liveHead;
        if (liveHead) liveHead->prev = header;
        liveHead = header;
        header->serial = static_cast<std::size_t>(serial);
#endif
    }

    /**
     * Runs on every delete. It takes the box out and hooks its two neighbors to each other.
     * Call with a ListGuard held.
     */
    void UnlinkBlock([[maybe_unused]] Header* header) noexcept
    {
#if MEMMON_CHECKED_HEADER
        if (header->prev) header->prev->next = header->next;
        if (header->next) header->next->prev = header->prev;
        if (header == liveHead) liveHead = header->next;
#endif
    }

#if MEMMON_CHECKED_HEADER
    /**
     * True if a block's serial came after the baseline.
     * Subtracting first keeps it right even when serial rolls over, like reading a clock face.
     * That holds while the baseline is less than half the counter's range old: never a problem
     * on x64, about 2 billion allocations on Win32.
     * @param serial the block's serial
     * @param baseline allocationCount from the baseline, cut to the same size as serial
     */
    bool IsAfter(const std::size_t serial, const std::size_t baseline) noexcept
    {
        return static_cast<std::make_signed_t<std::size_t>>(serial - baseline) > 0;
    }
#endif


    /* ============================== Printing ============================== */

    /**
     * Prints one colored line for a new or a delete.
     *
     * This runs inside operator new, so it must never allocate, or it calls itself forever.
     * That is why it writes into a stack buffer with format_to_n and prints with fputs:
     * - std::format returns a std::string, which allocates.
     * - std::cout allocates on first use, and is already gone while statics are destroyed.
     * Lines longer than 256 characters are cut off.
     *
     * Numbers must use a plain {}. In Debug, MSVC allocates for {:x}, {:b}, {:o} and for padded
     * numbers like {:08}. Padding a string, like {:<37}, is fine.
     * @param prefixColor
     * @param operation
     * @param functionColor
     * @param function
     * @param size
     * @param alignment
     * @param live bytes still allocated after this operation
     */
    void LogOperation(const std::string_view prefixColor, const std::string_view operation,
                      const std::string_view functionColor, const std::string_view function,
                      const std::optional<std::size_t> size = std::nullopt,
                      const std::optional<std::size_t> alignment = std::nullopt,
                      const std::optional<std::size_t> live = std::nullopt)
    {
        if (!loggingEnabled.load()) return;

        char buffer[256];
        /* Leave room for '\0' */
        const char* const end = buffer + sizeof(buffer) - 1;
        char* out = buffer;

        out = std::format_to_n(out, end - out, "{}Operation: {:<7}{}| {}{:<37}{}",
                               prefixColor, operation, MemColor::Reset,
                               functionColor, function, MemColor::Reset).out;
        if (size)
        {
            out = std::format_to_n(out, end - out, "| size = {}{} bytes{}",
                                   MemColor::Value, *size, MemColor::Reset).out;
        }
        if (alignment)
        {
            out = std::format_to_n(out, end - out, " | alignment = {}{}{}",
                                   MemColor::Value, *alignment, MemColor::Reset).out;
        }
        if (live)
        {
            out = std::format_to_n(out, end - out, " | live = {}{} bytes{}",
                                   MemColor::Value, *live, MemColor::Reset).out;
        }
        *out = '\0';

        std::fputs(buffer, stdout);
        std::fputc('\n', stdout);
    }

    /**
     * Prints one line of the report, without allocating
     * @param label
     * @param value
     * @param unit text printed after the value, for example " bytes"
     */
    template <typename T>
    void PrintStat(const std::string_view label, const T value, const std::string_view unit = "")
    {
        char buffer[128];
        const auto result = std::format_to_n(buffer, sizeof(buffer) - 1, "{:<20}: {}{}{}{}\n",
                                             label, MemColor::Value, value, MemColor::Reset, unit);
        *result.out = '\0';
        std::fputs(buffer, stdout);
    }


    /**
     * Prints a free the monitor refused. Always prints, even with logging off.
     * Must not allocate, same as LogOperation.
     * @param block the pointer handed to delete
     * @param check what was wrong with it
     * @param function the operator it came through
     */
    void LogBadBlock(const void* block, const BlockCheck check, const std::string_view function) noexcept
    {
        /*
         * "already freed once" rarely shows: the CRT usually overwrites a freed block right away,
         * so a double free normally gets the second message. Both mean: do not touch this pointer.
         */
        const std::string_view reason = check == BlockCheck::AlreadyFreed
                                            ? "already freed once"
                                            : "bad check value: freed twice, or not from here";

        char buffer[256];
        const char* const end = buffer + sizeof(buffer) - 1;
        char* out = buffer;

        out = std::format_to_n(out, end - out, "{}Refused:   {:<7}{}| {}{:<37}{}",
                               MemColor::Fail, "free", MemColor::Reset,
                               MemColor::Check, function, MemColor::Reset).out;
        out = std::format_to_n(out, end - out, "| address = {}{}{}",
                               MemColor::Value, block, MemColor::Reset).out;
        out = std::format_to_n(out, end - out, " | {}{}{}",
                               MemColor::Fail, reason, MemColor::Reset).out;
        *out = '\0';

        std::fputs(buffer, stdout);
        std::fputc('\n', stdout);
    }

    /**
     * Prints a free that came through the wrong delete. The block is still freed the right way,
     * because the header knows what it is. Always prints, even with logging off.
     * @param block
     * @param expected the alignment the delete assumed
     * @param actual the block's real alignment, 0 for a malloc block
     * @param function
     */
    void LogMismatch(const void* block, const std::size_t expected, const std::size_t actual,
                     const std::string_view function) noexcept
    {
        char buffer[256];
        const char* const end = buffer + sizeof(buffer) - 1;
        char* out = buffer;

        out = std::format_to_n(out, end - out, "{}Mismatch:  {:<7}{}| {}{:<37}{}",
                               MemColor::Fail, "free", MemColor::Reset,
                               MemColor::Check, function, MemColor::Reset).out;
        out = std::format_to_n(out, end - out, "| address = {}{}{}",
                               MemColor::Value, block, MemColor::Reset).out;
        out = std::format_to_n(out, end - out, " | expected align {}{}{}",
                               MemColor::Value, expected, MemColor::Reset).out;
        out = std::format_to_n(out, end - out, ", block says {}{}{}",
                               MemColor::Value, actual, MemColor::Reset).out;
        *out = '\0';

        std::fputs(buffer, stdout);
        std::fputc('\n', stdout);
    }

#if MEMMON_CHECKED_HEADER
    /**
     * Prints one leaked block for ReportLeaks. Must not allocate, same as LogOperation.
     * The address is the pointer the caller got from new, not the header's.
     * @param header
     */
    void LogLeak(const Header* header) noexcept
    {
        char buffer[256];
        const char* const end = buffer + sizeof(buffer) - 1;
        char* out = buffer;

        out = std::format_to_n(out, end - out, "{}Leak:{}      | address = {}{}{}",
                               MemColor::Fail, MemColor::Reset,
                               MemColor::Value, static_cast<const void*>(header + 1), MemColor::Reset).out;
        out = std::format_to_n(out, end - out, " | size = {}{} bytes{}",
                               MemColor::Value, header->numBytes, MemColor::Reset).out;
        if (header->alignment)
        {
            out = std::format_to_n(out, end - out, " | alignment = {}{}{}",
                                   MemColor::Value, header->alignment, MemColor::Reset).out;
        }

        if (header->file)
        {
            /* Only the file name: a full path can be long enough to cut the line off */
            const std::string_view path = header->file;
            const std::size_t slash = path.find_last_of("/\\");
            const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
            out = std::format_to_n(out, end - out, " | {}{}:{}{}",
                                   MemColor::Check, name, header->line, MemColor::Reset).out;
        }
        else
        {
            out = std::format_to_n(out, end - out, " | {}(unknown){}", MemColor::Section, MemColor::Reset).out;
        }
        *out = '\0';

        std::fputs(buffer, stdout);
        std::fputc('\n', stdout);
    }

    /**
     * Prints where the leak walk had to stop. Must not allocate, same as LogOperation.
     * @param header the block with the bad stamp
     */
    void LogDamagedBlock(const Header* header) noexcept
    {
        char buffer[256];
        const char* const end = buffer + sizeof(buffer) - 1;
        char* out = buffer;

        out = std::format_to_n(out, end - out, "{}Damaged:{}   | address = {}{}{}",
                               MemColor::Fail, MemColor::Reset,
                               MemColor::Value, static_cast<const void*>(header + 1), MemColor::Reset).out;
        out = std::format_to_n(out, end - out, " | {}bad check value, walk stopped: older blocks not listed{}",
                               MemColor::Fail, MemColor::Reset).out;
        *out = '\0';

        std::fputs(buffer, stdout);
        std::fputc('\n', stdout);
    }
#endif


    /* ============================== Counting ============================== */

    /**
     * What CountAllocation hands back
     */
    struct Counted
    {
        /** Bytes still allocated after this allocation */
        std::size_t live;
        /** This allocation's number, 1 for the first one. Becomes the block's serial. */
        std::uint64_t number;
    };

    /**
     * Raises target to value if value is bigger.
     * Another thread can change target between our read and our write. compare_exchange only
     * writes if target still holds what we read. If not, it puts the fresh value in seen,
     * and we try again.
     * @param target
     * @param value
     */
    void RaiseTo(std::atomic<std::size_t>& target, const std::size_t value) noexcept
    {
        std::size_t seen = target.load();
        while (seen < value)
        {
            /* On success we are done. On failure, seen now holds the newer value, so check again. */
            if (target.compare_exchange_weak(seen, value)) break;
        }
    }

    /**
     * Adds one allocation to the counters
     * @param size
     * @return the live bytes after this allocation, and its number
     */
    Counted CountAllocation(const std::size_t size) noexcept
    {
        /* fetch_add returns the value from before the add, so + 1 gives this allocation's own number */
        const std::uint64_t number = stats.allocationCount.fetch_add(1) + 1;
        stats.bytesAllocated.fetch_add(size);
        const std::size_t live = stats.liveBytes.fetch_add(size) + size;

        RaiseTo(stats.peakLiveBytes, live);
        RaiseTo(stats.largestAllocation, size);
        return {live, number};
    }

    /**
     * Adds one free to the counters
     * @param size
     * @return bytes still allocated after this free
     */
    std::size_t CountFree(const std::size_t size) noexcept
    {
        stats.freeCount.fetch_add(1);
        stats.bytesFreed.fetch_add(size);
        return stats.liveBytes.fetch_sub(size) - size;
    }


    /* ============================== Allocate / Free ============================== */

    /**
     * Allocates a block with a header in front, counts it and logs it
     * @param size bytes the caller asked for, header not included
     * @param functionColor
     * @param function
     * @param file source file of the new, nullptr if unknown
     * @param line source line of the new, 0 if unknown
     * @return pointer just past the header
     */
    void* Allocate(const std::size_t size, const std::string_view functionColor, const std::string_view function,
                   const char* file = nullptr, const std::uint32_t line = 0)
    {
        auto* header = static_cast<Header*>(std::malloc(sizeof(Header) + size));
        if (!header) throw std::bad_alloc();
        /* 0 marks it as a plain malloc block */
        InitHeader(header, size, 0, file, line);

        /* Number and link under one lock, so the list stays sorted by number for ReportLeaks */
        Counted counted{};
        {
            ListGuard guard;
            counted = CountAllocation(size);
            LinkBlock(header, counted.number);
        }
        LogOperation(MemColor::NewPrefix, "New", functionColor, function, size, std::nullopt, counted.live);
        return header + 1;
    }

    /**
     * Same as Allocate, for types that need more alignment than the default.
     * Layout: [ gap ... | header ][ caller's memory ]. The gap is HeaderSpace(alignment) bytes.
     * @param size bytes the caller asked for
     * @param alignment the returned pointer is a multiple of this
     * @param functionColor
     * @param function
     * @param file source file of the new, nullptr if unknown
     * @param line source line of the new, 0 if unknown
     * @return pointer just past the header, aligned as asked
     */
    void* AllocateAligned(const std::size_t size, const std::size_t alignment,
                          const std::string_view functionColor, const std::string_view function,
                          const char* file = nullptr, const std::uint32_t line = 0)
    {
        const std::size_t space = HeaderSpace(alignment);

        /* Aligning the whole block to space keeps base + space aligned too */
        auto* base = static_cast<std::byte*>(_aligned_malloc(space + size, space));
        if (!base) throw std::bad_alloc();

        /* The header sits right before the caller's memory */
        std::byte* block = base + space;
        InitHeader(reinterpret_cast<Header*>(block) - 1, size, alignment, file, line);

        Counted counted{};
        {
            ListGuard guard;
            counted = CountAllocation(size);
            LinkBlock(reinterpret_cast<Header*>(block) - 1, counted.number);
        }
        LogOperation(MemColor::NewPrefix, "New", functionColor, function, size, alignment, counted.live);
        return block;
    }

    /**
     * Frees a block from Allocate or AllocateAligned, counts it and logs it.
     *
     * The header picks free or _aligned_free, never the caller. So a block freed through the
     * wrong delete still goes back the right way.
     *
     * A block with a bad stamp is reported and not freed. Leaking one block is safe,
     * freeing a bad pointer is not.
     *
     * @param block pointer from Allocate or AllocateAligned, can be nullptr
     * @param functionColor
     * @param function
     * @param expectedAlignment what the calling delete assumes, 0 for the plain ones.
     *                          Only used to report a mismatch.
     */
    void Free(void* block, const std::string_view functionColor, const std::string_view function,
              const std::size_t expectedAlignment) noexcept
    {
        if (!block) return;
        Header* header = static_cast<Header*>(block) - 1;

        BlockCheck check;
        {
            ListGuard guard;
            check = CheckHeader(header);
            if (check == BlockCheck::Ok)
            {
                UnlinkBlock(header);
                MarkFreed(header);
            }
        }

        if (check != BlockCheck::Ok)
        {
            ++stats.badBlockCount;
            LogBadBlock(block, check, function);
            /* Leaking one block beats corrupting the heap */
            return;
        }

        /* The block is ours alone now: out of the list and stamped freed */
        const std::size_t alignment = header->alignment;
        if (alignment != expectedAlignment)
        {
            ++stats.mismatchCount;
            LogMismatch(block, expectedAlignment, alignment, function);
        }

        const std::size_t live = CountFree(header->numBytes);
        LogOperation(MemColor::DeletePrefix, "Delete", functionColor, function, header->numBytes,
                     alignment ? std::optional{alignment} : std::nullopt, live);
        if (alignment == 0) std::free(header);
        else _aligned_free(static_cast<std::byte*>(block) - HeaderSpace(alignment));
    }
}


/* ============================== Public API ============================== */

MemoryStats MemoryMonitor::GetStats()
{
    /* One load per field. They are not all from the same moment yet, step 4 fixes that. */
    MemoryStats copy;
    copy.allocationCount = stats.allocationCount.load();
    copy.freeCount = stats.freeCount.load();
    copy.bytesAllocated = stats.bytesAllocated.load();
    copy.bytesFreed = stats.bytesFreed.load();
    copy.peakLiveBytes = stats.peakLiveBytes.load();
    copy.largestAllocation = stats.largestAllocation.load();
    copy.badBlockCount = stats.badBlockCount.load();
    copy.mismatchCount = stats.mismatchCount.load();
    return copy;
}

void MemoryMonitor::SetLogging(const bool enabled)
{
    loggingEnabled.store(enabled);
}

bool MemoryMonitor::IsLogging()
{
    return loggingEnabled.load();
}

std::size_t MemoryMonitor::HeaderOverhead()
{
    return sizeof(Header);
}

bool MemoryMonitor::HasCheckedHeaders()
{
    return MEMMON_CHECKED_HEADER != 0;
}

void MemoryMonitor::Report(const MemoryStats& baseline)
{
    /* Copy first so every line reads the same numbers */
    const MemoryStats now = GetStats();

    const std::uint64_t allocations = now.allocationCount - baseline.allocationCount;
    const std::uint64_t frees = now.freeCount - baseline.freeCount;
    const std::uint64_t bytesAllocated = now.bytesAllocated - baseline.bytesAllocated;
    const std::uint64_t bytesFreed = now.bytesFreed - baseline.bytesFreed;

    /* Signed: code after the baseline can free memory allocated before it */
    const std::int64_t liveBytes = static_cast<std::int64_t>(bytesAllocated) - static_cast<std::int64_t>(bytesFreed);
    const std::int64_t liveBlocks = static_cast<std::int64_t>(allocations) - static_cast<std::int64_t>(frees);

    char title[64];
    *std::format_to_n(title, sizeof(title) - 1, "{}-- Memory Report --{}\n", MemColor::Section, MemColor::Reset).out
        = '\0';
    std::fputs(title, stdout);

    PrintStat("Allocations", allocations);
    PrintStat("Frees", frees);
    PrintStat("Bytes allocated", bytesAllocated, " bytes");
    PrintStat("Bytes freed", bytesFreed, " bytes");
    PrintStat("Live bytes", liveBytes, " bytes");
    PrintStat("Live blocks", liveBlocks);
    PrintStat("Average allocation", allocations ? bytesAllocated / allocations : 0, " bytes");
    PrintStat("Header size", sizeof(Header), " bytes");
    PrintStat("Header alignment", alignof(Header));
    PrintStat("Default new align", __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    PrintStat("Peak live bytes", now.peakLiveBytes, " bytes (whole program)");
    PrintStat("Largest allocation", now.largestAllocation, " bytes (whole program)");

    /* Only shown when something went wrong */
    const std::uint64_t badBlocks = now.badBlockCount - baseline.badBlockCount;
    const std::uint64_t mismatches = now.mismatchCount - baseline.mismatchCount;
    if (badBlocks) PrintStat("Refused frees", badBlocks, " (bad or already freed)");
    if (mismatches) PrintStat("Wrong operator", mismatches, " (freed through the wrong delete)");
}

LeakSummary MemoryMonitor::ReportLeaks([[maybe_unused]] const MemoryStats& baseline)
{
    char title[64];
    *std::format_to_n(title, sizeof(title) - 1, "{}-- Leak Report --{}\n", MemColor::Section, MemColor::Reset).out
        = '\0';
    std::fputs(title, stdout);

    LeakSummary summary;

#if MEMMON_CHECKED_HEADER
    /* A default baseline means "list everything", so there is nothing to compare against */
    const bool everything = baseline.allocationCount == 0;
    const auto since = static_cast<std::size_t>(baseline.allocationCount);

    ListGuard guard;
    for (const Header* header = liveHead; header; header = header->next)
    {
        /* Check the stamp first. A bad one means next cannot be trusted either. */
        if (CheckHeader(header) != BlockCheck::Ok)
        {
            summary.damaged = true;
            LogDamagedBlock(header);
            break;
        }

        /* Newest first, so the first block from before the baseline means all the rest are too */
        if (!everything && !IsAfter(header->serial, since)) break;

        LogLeak(header);
        ++summary.listed;
        summary.bytes += header->numBytes;
    }

    PrintStat("Leaked blocks", summary.listed);
    PrintStat("Leaked bytes", summary.bytes, " bytes");
#else
    PrintStat("Leak list", "needs a Debug build");
#endif
    return summary;
}

AllocationSite MemoryMonitor::GetAllocationSite([[maybe_unused]] const void* block)
{
#if MEMMON_CHECKED_HEADER
    if (!block) return {};

    const Header* header = static_cast<const Header*>(block) - 1;

    /* A bad stamp means the rest of the header cannot be trusted either */
    if (CheckHeader(header) != BlockCheck::Ok) return {};

    return {header->file, header->line};
#else
    return {};
#endif
}


/* ============================== Operator replacements: new ============================== */

void* operator new(std::size_t _Size)
{
    return Allocate(_Size, MemColor::Plain, "new(size_t)");
}

void* operator new [](std::size_t _Size)
{
    return Allocate(_Size, MemColor::PlainArray, "new[](size_t)");
}

void* operator new(std::size_t _Size, std::align_val_t _Al)
{
    return AllocateAligned(_Size, static_cast<std::size_t>(_Al), MemColor::Aligned, "new(size_t, align_val_t)");
}

void* operator new[](std::size_t _Size, std::align_val_t _Al)
{
    return AllocateAligned(_Size, static_cast<std::size_t>(_Al), MemColor::AlignedArray, "new[](size_t, align_val_t)");
}


/* ============================== Operator replacements: delete ============================== */

void operator delete(void* _Block) noexcept
{
    Free(_Block, MemColor::Plain, "delete(void*)", 0);
}

void operator delete(void* _Block, std::size_t _Size) noexcept
{
    Free(_Block, MemColor::Sized, "delete(void*, size_t)", 0);
}

void operator delete [](void* _Block) noexcept
{
    Free(_Block, MemColor::PlainArray, "delete[](void*)", 0);
}

void operator delete[](void* _Block, std::size_t _Size) noexcept
{
    Free(_Block, MemColor::SizedArray, "delete[](void*, size_t)", 0);
}

/* The aligned deletes pass their alignment only so Free can report a mismatch */
void operator delete(void* _Block, std::align_val_t _Al) noexcept
{
    Free(_Block, MemColor::Aligned, "delete(void*, align_val_t)",
         static_cast<std::size_t>(_Al));
}

void operator delete(void* _Block, std::size_t _Size,
                     std::align_val_t _Al) noexcept
{
    Free(_Block, MemColor::SizedAligned, "delete(void*, size_t, align_val_t)",
         static_cast<std::size_t>(_Al));
}

void operator delete[](void* _Block, std::align_val_t _Al) noexcept
{
    Free(_Block, MemColor::AlignedArray, "delete[](void*, align_val_t)",
         static_cast<std::size_t>(_Al));
}

void operator delete[](void* _Block, std::size_t _Size,
                       std::align_val_t _Al) noexcept
{
    Free(_Block, MemColor::SizedAlignedArray, "delete[](void*, size_t, align_val_t)",
         static_cast<std::size_t>(_Al));
}

/* ============================== File-tagged new / delete (Debug only) ============================== */

#if MEMMON_CHECKED_HEADER
void* operator new(std::size_t _Size, const char* file, int line)
{
    return Allocate(_Size, MemColor::Plain, "new(size_t) file-tagged",
                    file, static_cast<std::uint32_t>(line));
}

void* operator new[](std::size_t _Size, const char* file, int line)
{
    return Allocate(_Size, MemColor::PlainArray, "new[](size_t) file-tagged",
                    file, static_cast<std::uint32_t>(line));
}

void* operator new(std::size_t _Size, std::align_val_t _Al, const char* file, int line)
{
    return AllocateAligned(_Size, static_cast<std::size_t>(_Al), MemColor::Aligned,
                           "new(size_t, align_val_t) file-tagged", file, static_cast<std::uint32_t>(line));
}

void* operator new[](std::size_t _Size, std::align_val_t _Al, const char* file, int line)
{
    return AllocateAligned(_Size, static_cast<std::size_t>(_Al), MemColor::AlignedArray,
                           "new[](size_t, align_val_t) file-tagged", file, static_cast<std::uint32_t>(line));
}

/* Only reached when a constructor throws. File and line are not needed to free. */
void operator delete(void* _Block, const char*, int) noexcept
{
    Free(_Block, MemColor::Plain, "delete(void*) file-tagged", 0);
}

void operator delete[](void* _Block, const char*, int) noexcept
{
    Free(_Block, MemColor::PlainArray, "delete[](void*) file-tagged", 0);
}

void operator delete(void* _Block, std::align_val_t _Al, const char*, int) noexcept
{
    Free(_Block, MemColor::Aligned, "delete(void*, align_val_t) file-tagged",
         static_cast<std::size_t>(_Al));
}

void operator delete[](void* _Block, std::align_val_t _Al, const char*, int) noexcept
{
    Free(_Block, MemColor::AlignedArray, "delete[](void*, align_val_t) file-tagged",
         static_cast<std::size_t>(_Al));
}
#endif


/* ============================== Not replaced yet ============================== */

/*
 * TODO: the nothrow overloads. MSVC forwards them to the ones above, other compilers may not.
 * void* operator new(std::size_t _Size, const std::nothrow_t&) noexcept
 * void* operator new[](std::size_t _Size, const std::nothrow_t&) noexcept
 * void* operator new(std::size_t _Size, std::align_val_t _Al, const std::nothrow_t&) noexcept
 * void* operator new[](std::size_t _Size, std::align_val_t _Al, const std::nothrow_t&) noexcept
 * void operator delete(void* _Block, const std::nothrow_t&) noexcept
 * void operator delete[](void* _Block, const std::nothrow_t&) noexcept
 * void operator delete(void* _Block, std::align_val_t _Al, const std::nothrow_t&) noexcept
 * void operator delete[](void* _Block, std::align_val_t _Al, const std::nothrow_t&) noexcept
 */
