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


    // ============================== Block layout ==============================

    /**
     * Stored in front of every block, used to access the number of bytes on delete
     * __STDCPP_DEFAULT_NEW_ALIGNMENT__ That makes it 16 on x64 and 8 on x86
     */
    struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) Header
    {
        std::size_t numBytes;
    };
    static_assert(sizeof(Header) >= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    static_assert((sizeof(Header) & (sizeof(Header) - 1)) == 0, "Header size must be a power of two");

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
        header->numBytes = size;

        const std::size_t live = CountAllocation(size);
        LogOperation(MemColor::NewPrefix, "New", functionColor, function, size, std::nullopt, live);
        return header + 1;
    }

    /**
     * Frees a block made by Allocate, reads its size from the Header, updates the counters and logs the operation
     * @param block pointer returned by Allocate, can be nullptr
     * @param functionColor
     * @param function
     */
    void Free(void* block, const std::string_view functionColor, const std::string_view function) noexcept
    {
        if (!block) return;
        Header* header = static_cast<Header*>(block) - 1;

        const std::size_t live = CountFree(header->numBytes);
        LogOperation(MemColor::DeletePrefix, "Delete", functionColor, function, header->numBytes, std::nullopt, live);
        std::free(header);
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
        (reinterpret_cast<Header*>(block) - 1)->numBytes = size; // Header sits right before the caller's memory

        const std::size_t live = CountAllocation(size);
        LogOperation(MemColor::NewPrefix, "New", functionColor, function, size, alignment, live);
        return block;
    }

    /**
     * Frees a block made by AllocateAligned
     * @param block pointer returned by AllocateAligned, can be nullptr
     * @param alignment the same value the matching new was given
     * @param functionColor
     * @param function
     */
    void FreeAligned(void* block, const std::size_t alignment,
                     const std::string_view functionColor, const std::string_view function) noexcept
    {
        if (!block) return;
        const Header* header = static_cast<Header*>(block) - 1;

        const std::size_t live = CountFree(header->numBytes);
        LogOperation(MemColor::DeletePrefix, "Delete", functionColor, function, header->numBytes, alignment, live);
        _aligned_free(static_cast<std::byte*>(block) - HeaderSpace(alignment));
    }
}


// ============================== Public API ==============================

MemoryStats MemoryMonitor::GetStats()
{
    return stats;
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
    Free(_Block, MemColor::Plain, "delete(void*)");
}

void operator delete(void* _Block, std::size_t _Size) noexcept
{
    Free(_Block, MemColor::Sized, "delete(void*, size_t)");
}

void operator delete [](void* _Block) noexcept
{
    Free(_Block, MemColor::PlainArray, "delete[](void*)");
}

void operator delete[](void* _Block, std::size_t _Size) noexcept
{
    Free(_Block, MemColor::SizedArray, "delete[](void*, size_t)");
}

// Aligned overloads: the memory comes from AllocateAligned, so it must go back through _aligned_free.
void operator delete(void* _Block, std::align_val_t _Al) noexcept
{
    FreeAligned(_Block, static_cast<std::size_t>(_Al), MemColor::Aligned, "delete(void*, align_val_t)");
}

void operator delete(void* _Block, std::size_t _Size,
                     std::align_val_t _Al) noexcept
{
    FreeAligned(_Block, static_cast<std::size_t>(_Al), MemColor::SizedAligned, "delete(void*, size_t, align_val_t)");
}

void operator delete[](void* _Block, std::align_val_t _Al) noexcept
{
    FreeAligned(_Block, static_cast<std::size_t>(_Al), MemColor::AlignedArray, "delete[](void*, align_val_t)");
}

void operator delete[](void* _Block, std::size_t _Size,
                       std::align_val_t _Al) noexcept
{
    FreeAligned(_Block, static_cast<std::size_t>(_Al), MemColor::SizedAlignedArray,
                "delete[](void*, size_t, align_val_t)");
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
