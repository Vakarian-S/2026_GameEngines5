#pragma once
#include <cstddef>
#include <cstdint>
/* std::unique_ptr, for MEMMON_MAKE_UNIQUE */
#include <memory>
#include <new>
#include <string_view>


/* ============================== Debug Check ============================== */

/* Debug headers carry extra fields for checking. Release keeps the header small. */
#ifdef _DEBUG
#define MEMMON_CHECKED_HEADER 1
#else
#define MEMMON_CHECKED_HEADER 0
#endif

/* ============================== File-tagged new ============================== */

#if MEMMON_CHECKED_HEADER
/*
 * new(__FILE__, __LINE__) calls these. They store the file and line in the block's header,
 * so ReportLeaks can say where a leak came from.
 */
void* operator new(std::size_t size, const char* file, int line);
void* operator new[](std::size_t size, const char* file, int line);
void* operator new(std::size_t size, std::align_val_t alignment, const char* file, int line);
void* operator new[](std::size_t size, std::align_val_t alignment, const char* file, int line);

/*
 * Only called when a constructor throws. C++ then frees the memory with the delete
 * that takes the same extra inputs as the new.
 */
void operator delete(void* block, const char* file, int line) noexcept;
void operator delete[](void* block, const char* file, int line) noexcept;
void operator delete(void* block, std::align_val_t alignment, const char* file, int line) noexcept;
void operator delete[](void* block, std::align_val_t alignment, const char* file, int line) noexcept;

/* Debug - Saves the current File and Line */
#define MEMMON_NEW new(__FILE__, __LINE__)

/**
 * Like std::make_unique, but the block remembers this file and line, so a leak can name it.
 * std::make_unique writes its own new inside the standard library, which no file tag can reach.
 *
 * One object only. For arrays use std::make_unique<Type[]>(count), which stays untagged.
 *
 * Debug: one expression, so the pointer owns the object right away, as safe as make_unique.
 * Release: plain std::make_unique, since nothing is recorded there anyway.
 */
#define MEMMON_MAKE_UNIQUE(Type, ...) std::unique_ptr<Type>(MEMMON_NEW Type(__VA_ARGS__))
#else
/* Release - Does nothing */
#define MEMMON_NEW new

/** Release - the plain std::make_unique */
#define MEMMON_MAKE_UNIQUE(Type, ...) std::make_unique<Type>(__VA_ARGS__)
#endif


/* ============================== Console colors ============================== */

/**
 * Color Codes for making the terminal easier to read, each Function and Action has its own color
 */
namespace MEM_COLOR
{
    /** Back to the default color */
    inline constexpr std::string_view kReset = "\x1b[0m";
    /** Dim gray */
    inline constexpr std::string_view kSection = "\x1b[90m";
    /** Bold white */
    inline constexpr std::string_view kValue = "\x1b[1;97m";

    /** Bold green */
    inline constexpr std::string_view kNewPrefix = "\x1b[1;32m";
    /** Bold red */
    inline constexpr std::string_view kDeletePrefix = "\x1b[1;31m";

    /* Scalar overloads use the normal color, array overloads use the bright one. */

    /** Cyan */
    inline constexpr std::string_view kPlain = "\x1b[36m";
    /** Bright cyan */
    inline constexpr std::string_view kPlainArray = "\x1b[96m";
    /** Yellow */
    inline constexpr std::string_view kSized = "\x1b[33m";
    /** Bright yellow */
    inline constexpr std::string_view kSizedArray = "\x1b[93m";
    /** Magenta */
    inline constexpr std::string_view kAligned = "\x1b[35m";
    /** Bright magenta */
    inline constexpr std::string_view kAlignedArray = "\x1b[95m";
    /** Blue */
    inline constexpr std::string_view kSizedAligned = "\x1b[34m";
    /** Bright blue */
    inline constexpr std::string_view kSizedAlignedArray = "\x1b[94m";

    /* Alignment checks */

    /** Bold green */
    inline constexpr std::string_view kOk = "\x1b[1;32m";
    /** Bold bright red */
    inline constexpr std::string_view kFail = "\x1b[1;91m";
    /** Bright cyan */
    inline constexpr std::string_view kCheck = "\x1b[96m";
}

/* ============================== Public API ============================== */

/**
 * A copy of the heap counters at one point in time
 */
struct MemoryStats
{
    /* Running totals, they only grow. 64-bit so they can't wrap on Win32. */

    /** Total calls to new */
    std::uint64_t allocationCount = 0;
    /** Total calls to delete that really freed */
    std::uint64_t freeCount = 0;
    /** Total bytes asked for */
    std::uint64_t bytesAllocated = 0;
    /** Total bytes given back */
    std::uint64_t bytesFreed = 0;

    /** Most memory alive at once */
    std::size_t peakLiveBytes = 0;
    /** Biggest single block */
    std::size_t largestAllocation = 0;

    /* Problems the monitor caught. Both stay 0 for correct code. */

    /** Frees refused because the block's stamp was wrong */
    std::uint64_t badBlockCount = 0;
    /** Frees that came through the wrong delete */
    std::uint64_t mismatchCount = 0;

    /** Bytes still allocated. The cast is safe: live memory always fits in size_t. */
    std::size_t LiveBytes() const { return static_cast<std::size_t>(bytesAllocated - bytesFreed); }
    /** Blocks still allocated */
    std::uint64_t LiveBlocks() const { return allocationCount - freeCount; }
};

/**
 * What ReportLeaks() finds
 */
struct LeakSummary
{
    /** Blocks listed */
    std::size_t listed = 0;
    /** Total bytes of the blocks listed */
    std::size_t bytes = 0;
    /** True if the walk hit a damaged block and stopped early */
    bool damaged = false;
};

/**
 * Where a block was allocated
 */
struct AllocationSite
{
    /** Source file of the new. nullptr for a plain new, a bad pointer, or in Release. */
    const char* file = nullptr;
    /** Source line of the new, 0 when file is nullptr */
    std::uint32_t line = 0;
};


class MemoryMonitor
{
public:
    /**
     * @return a copy of the current counters
     */
    static MemoryStats GetStats();

    /**
     * Turns the per-allocation log lines on or off. Counting never stops, only the printing.
     * Report() is not affected, it always prints.
     * @param enabled
     */
    static void SetLogging(bool enabled);

    /**
     * @return true if the per-allocation log lines are on
     */
    static bool IsLogging();

    /**
     * @return bytes the monitor reserves in front of every block. Larger in Debug, which carries
     *         the extra fields used for checking.
     */
    static std::size_t HeaderOverhead();

    /**
     * Blocks carry a check value in Debug, so double frees and small underruns are caught and refused.
     * Release drops it, and a bad free goes through unnoticed like it would without the monitor.
     * @return true if blocks are checked
     */
    static bool HasCheckedHeaders();

    /**
     * Prints the counters since the baseline. Peak and largest always cover the whole program.
     * @param baseline stats saved earlier with GetStats(), for example at the start of main
     */
    static void Report(const MemoryStats& baseline = {});

    /**
     * Prints every block still alive that was allocated after the baseline, newest first.
     * Always prints, even with logging off. Debug only: Release has no leak list and returns zeros.
     * @param baseline stats saved earlier with GetStats(). The default lists every live block,
     *                 including the runtime's own from before main.
     * @return how many blocks and bytes were listed
     */
    static LeakSummary ReportLeaks(const MemoryStats& baseline = {});

    /**
     * Reads where a block was allocated. Debug only: Release always returns an empty site.
     * Like delete, only safe on pointers that came from new.
     * @param block pointer returned by new
     * @return file and line, or an empty site if the block was not made with MEMMON_NEW
     */
    static AllocationSite GetAllocationSite(const void* block);
};
