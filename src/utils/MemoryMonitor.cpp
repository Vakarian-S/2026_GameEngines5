#include "utils/MemoryMonitor.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <malloc.h> // _aligned_malloc / _aligned_free
#include <optional>


namespace
{
    // ============================== Counters ==============================

    // constinit: set to zero before any code runs, so allocations before main are counted safely
    constinit MemoryStats stats{};

    // Starts on with constinit, so the allocations the runtime makes before main are printed too.
    constinit bool loggingEnabled = true;


    // ============================== Block layout ==============================

    // Debug blocks carry extra fields for checking. Release stays lean.
    // Nothing crosses between builds, so the two sizes can differ safely.
#ifdef _DEBUG
#define MEMMON_CHECKED_HEADER 1
#else
#define MEMMON_CHECKED_HEADER 0
#endif

#if MEMMON_CHECKED_HEADER
    /**
     * Written into every block at allocation, replaced at free. One field doing two jobs:
     * any other value means the pointer did not come from here, and FreedMagic means it was
     * already deleted once.
     */
    constexpr std::uint32_t LiveMagic = 0xC0FFEEu;
    constexpr std::uint32_t FreedMagic = 0xDEAD5EEDu;

    // The next power of two that holds the extra fields. Not negotiable: HeaderSpace can hand
    // sizeof(Header) straight to _aligned_malloc, which kills the process on a non power of two.
    constexpr std::size_t CheckedHeaderSize = 2 * __STDCPP_DEFAULT_NEW_ALIGNMENT__;

    // Filler that pushes magic to the very end of the header
    // 
    // The Header struct contains: numBytes (size_t), alignment (size_t), reserved (uint32_t[]), magic (uint32_t)
    // We know the total size must be CheckedHeaderSize (a power of two, large enough for all fields).
    // We need to calculate how many uint32_t elements fit in the reserved array.
    //
    // Math breakdown:
    //   - Start with CheckedHeaderSize (total bytes available)
    //   - Subtract 2 * sizeof(size_t) for numBytes and alignment fields
    //   - Subtract sizeof(uint32_t) for the magic field at the end
    //   - Divide the remaining bytes by sizeof(uint32_t) to get the array count
    //
    // This ensures magic sits at the very end: any small underrun will hit it instead of random padding.

    constexpr std::size_t HeaderPadWords =
        (CheckedHeaderSize - 2 * sizeof(std::size_t) - sizeof(std::uint32_t)) / sizeof(std::uint32_t);
#endif

    /**
     * Stored in front of every block, used to access the number of bytes on delete.
     * alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) makes it at least 16 on x64 and 8 on x86.
     * alignment is 0 for a malloc block and the real value for an _aligned_malloc one, so delete
     * can pick the matching free without trusting the caller.
     */
    struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) Header
    {
        std::size_t numBytes;
        std::size_t alignment;
#if MEMMON_CHECKED_HEADER
        std::uint32_t reserved[HeaderPadWords];
        std::uint32_t magic;
#endif
    };

    static_assert(sizeof(Header) >= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    static_assert((sizeof(Header) & (sizeof(Header) - 1)) == 0, "Header size must be a power of two");
#if MEMMON_CHECKED_HEADER
    static_assert(sizeof(Header) == CheckedHeaderSize);
    static_assert(offsetof(Header, magic) + sizeof(Header::magic) == sizeof(Header),
                  "magic must be the last bytes of the header, so a small underrun lands on it");
#endif

    enum class BlockCheck { Ok, NotOurs, AlreadyFreed };

    /**
     * Reads the check value a block was stamped with.
     * A truly wild pointer can still fault on this read, before there is anything to report.
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
     * Fills in a fresh header
     * @param header
     * @param size bytes the caller asked for
     * @param alignment 0 for a malloc block, the real value for an _aligned_malloc one
     */
    void InitHeader(Header* header, const std::size_t size, const std::size_t alignment) noexcept
    {
        header->numBytes = size;
        header->alignment = alignment;
#if MEMMON_CHECKED_HEADER
        header->magic = LiveMagic;
#endif
    }

    /**
     * Clears the check value on the way out, so a second delete of the same pointer is caught
     * instead of guessed at
     * @param header
     */
    void MarkFreed([[maybe_unused]] Header* header) noexcept
    {
#if MEMMON_CHECKED_HEADER
        header->magic = FreedMagic;
#endif
    }

    /**
     * Bytes reserved in front of the caller's memory for an aligned block.
     * It is the alignment itself, so the caller's pointer stays a multiple of it,
     * but never smaller than the Header. Both values are powers of two, so the result is one too.
     * @param alignment
     */
    std::size_t HeaderSpace(const std::size_t alignment)
    {
        return alignment > sizeof(Header) ? alignment : sizeof(Header);
    }


    // ============================== Printing ==============================

    /**
     * Logs allocation and deletion using console colors.
     *
     * Not std::format and not std::cout, because this runs inside operator new and must not touch the heap:
     * - std::format returns a std::string, and a string of this length allocates, which calls operator new again,
     *   which logs again... forever. format_to_n writes into the stack buffer below instead, so nothing is allocated.
     * - std::cout allocates its buffer on first use, and it is gone during static destruction. fputs uses the C
     *   stdio buffer, which the runtime sets up before main and keeps until the very end.
     *
     * The cost is the fixed buffer: anything past 256 characters is cut off.
     *
     * Numbers must use a plain {} here. In a Debug build MSVC allocates 16 bytes for any other base
     * ({:x}, {:b}, {:o}) and for padded numbers ({:08}, {:>8}), because those go through an internal temporary.
     * That allocation would land inside operator new and recurse straight back into this function.
     * Padding a string, like {:<37} below, is free and safe.
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
        if (!loggingEnabled) return;

        char buffer[256];
        const char* const end = buffer + sizeof(buffer) - 1; // leave room for '\0'
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
     * Prints a free the monitor refused to carry out. Always prints, even with logging off
     * Same stack-buffer rules as LogOperation, nothing in here may allocate.
     * @param block the pointer that was handed to delete
     * @param check what was wrong with it
     * @param function the operator it came through
     */
    void LogBadBlock(const void* block, const BlockCheck check, const std::string_view function) noexcept
    {
        // AlreadyFreed only survive    s while nothing has reused the block. The CRT usually writes its
        // own free list over it straight away, so a real double free normally shows up as the
        // vaguer message below. Both mean the same thing: do not touch this pointer.
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
     * Prints a free that came through the wrong operator. The block is still freed correctly,
     * because the header knows what it is. Always prints, for the same reason as above.
     * @param block
     * @param expected what the calling operator assumed
     * @param actual what the block really is, 0 meaning a plain malloc block
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


    // ============================== Counting ==============================

    /**
     * Adds one allocation to the counters
     * @param size
     * @return bytes still allocated after this allocation
     */
    std::size_t CountAllocation(const std::size_t size)
    {
        ++stats.allocationCount;
        stats.bytesAllocated += size;

        const std::size_t live = stats.LiveBytes();
        stats.peakLiveBytes = std::max(live, stats.peakLiveBytes);
        stats.largestAllocation = std::max(size, stats.largestAllocation);
        return live;
    }

    /**
     * Adds one deallocation to the counters
     * @param size
     * @return bytes still allocated after this deallocation
     */
    std::size_t CountFree(const std::size_t size)
    {
        ++stats.freeCount;
        stats.bytesFreed += size;
        return stats.LiveBytes();
    }


    // ============================== Allocate / Free ==============================

    /**
     * Allocates a block with a Header in front, updates the counters and logs the operation
     * @param size bytes requested by the caller, not counting the Header
     * @param functionColor
     * @param function
     * @return pointer to the memory just past the Header
     */
    void* Allocate(const std::size_t size, const std::string_view functionColor, const std::string_view function)
    {
        auto* header = static_cast<Header*>(std::malloc(sizeof(Header) + size));
        if (!header) throw std::bad_alloc();
        InitHeader(header, size, 0); // 0 marks it as a plain malloc block

        const std::size_t live = CountAllocation(size);
        LogOperation(MemColor::NewPrefix, "New", functionColor, function, size, std::nullopt, live);
        return header + 1;
    }

    /**
     * Same as Allocate, for types that need more alignment than malloc's 16 bytes.
     * Layout: [ padding ... | Header ][ caller's memory ], the gap is HeaderSpace(alignment) bytes.
     * @param size bytes requested by the caller
     * @param alignment the caller's pointer is a multiple of this
     * @param functionColor
     * @param function
     * @return pointer to the memory just past the Header, aligned as asked
     */
    void* AllocateAligned(const std::size_t size, const std::size_t alignment,
                          const std::string_view functionColor, const std::string_view function)
    {
        const std::size_t space = HeaderSpace(alignment);

        // Align the whole block to space, so base + space is a multiple of both space and alignment
        auto* base = static_cast<std::byte*>(_aligned_malloc(space + size, space));
        if (!base) throw std::bad_alloc();

        std::byte* block = base + space;
        InitHeader(reinterpret_cast<Header*>(block) - 1, size, alignment);
        // Header sits right before the caller's memory

        const std::size_t live = CountAllocation(size);
        LogOperation(MemColor::NewPrefix, "New", functionColor, function, size, alignment, live);
        return block;
    }

    /**
     * Frees a block made by Allocate or AllocateAligned, updates the counters and logs the operation.
     *
     * The header decides which free to call, never the caller. That is the whole point: a block
     * freed through the wrong operator, or with an alignment that does not match the one it was
     * given, still goes back to the right place.
     *
     * A block whose check value is wrong is reported and then left alone. Leaking one block is
     * recoverable, handing a bad pointer to free is not.
     *
     * @param block pointer returned by Allocate or AllocateAligned, can be nullptr
     * @param functionColor
     * @param function
     * @param expectedAlignment what the calling operator assumes, 0 for the plain deletes.
     *                          Only used to report a disagreement, never to find the memory.
     */
    void Free(void* block, const std::string_view functionColor, const std::string_view function,
              const std::size_t expectedAlignment) noexcept
    {
        if (!block) return;
        Header* header = static_cast<Header*>(block) - 1;

        if (const BlockCheck check = CheckHeader(header); check != BlockCheck::Ok)
        {
            ++stats.badBlockCount;
            LogBadBlock(block, check, function);
            return; // leaking one block beats corrupting the heap
        }

        const std::size_t alignment = header->alignment;
        if (alignment != expectedAlignment)
        {
            ++stats.mismatchCount;
            LogMismatch(block, expectedAlignment, alignment, function);
        }

        const std::size_t live = CountFree(header->numBytes);
        LogOperation(MemColor::DeletePrefix, "Delete", functionColor, function, header->numBytes,
                     alignment ? std::optional{alignment} : std::nullopt, live);

        MarkFreed(header);

        if (alignment == 0) std::free(header);
        else _aligned_free(static_cast<std::byte*>(block) - HeaderSpace(alignment));
    }
}


// ============================== Public API ==============================

MemoryStats MemoryMonitor::GetStats()
{
    return stats;
}

void MemoryMonitor::SetLogging(const bool enabled)
{
    loggingEnabled = enabled;
}

bool MemoryMonitor::IsLogging()
{
    return loggingEnabled;
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
    // Copy first so every line reads the same numbers
    const MemoryStats now = stats;

    const std::uint64_t allocations = now.allocationCount - baseline.allocationCount;
    const std::uint64_t frees = now.freeCount - baseline.freeCount;
    const std::uint64_t bytesAllocated = now.bytesAllocated - baseline.bytesAllocated;
    const std::uint64_t bytesFreed = now.bytesFreed - baseline.bytesFreed;

    // Signed: code after the baseline can free memory that was allocated before it
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

    // Only shown when something went wrong
    const std::uint64_t badBlocks = now.badBlockCount - baseline.badBlockCount;
    const std::uint64_t mismatches = now.mismatchCount - baseline.mismatchCount;
    if (badBlocks) PrintStat("Refused frees", badBlocks, " (bad or already freed)");
    if (mismatches) PrintStat("Wrong operator", mismatches, " (freed through the wrong delete)");
}


// ============================== Operator replacements: new ==============================

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


// ============================== Operator replacements: delete ==============================

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

// Aligned overloads: they pass their alignment only so Free can report a mismatch. The header picks the free function.
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


// ============================== Not replaced yet ==============================

// TODO: missing nothrow overloads (MSVC forwards these to the ones above, but other compilers may not)
// void* operator new(std::size_t _Size, const std::nothrow_t&) noexcept
// void* operator new[](std::size_t _Size, const std::nothrow_t&) noexcept
// void* operator new(std::size_t _Size, std::align_val_t _Al, const std::nothrow_t&) noexcept
// void* operator new[](std::size_t _Size, std::align_val_t _Al, const std::nothrow_t&) noexcept
// void operator delete(void* _Block, const std::nothrow_t&) noexcept
// void operator delete[](void* _Block, const std::nothrow_t&) noexcept
// void operator delete(void* _Block, std::align_val_t _Al, const std::nothrow_t&) noexcept
// void operator delete[](void* _Block, std::align_val_t _Al, const std::nothrow_t&) noexcept
