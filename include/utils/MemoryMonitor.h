#pragma once
#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>


/* ============================== Console colors ============================== */

/**
 * Color Codes for making the terminal easier to read, each Function and Action has its own color
 */
namespace MemColor
{
    /** Back to the default color */
    inline constexpr std::string_view Reset = "\x1b[0m";
    /** Dim gray */
    inline constexpr std::string_view Section = "\x1b[90m";
    /** Bold white */
    inline constexpr std::string_view Value = "\x1b[1;97m";

    /** Bold green */
    inline constexpr std::string_view NewPrefix = "\x1b[1;32m";
    /** Bold red */
    inline constexpr std::string_view DeletePrefix = "\x1b[1;31m";

    /* Scalar overloads use the normal color, array overloads use the bright one. */

    /** Cyan */
    inline constexpr std::string_view Plain = "\x1b[36m";
    /** Bright cyan */
    inline constexpr std::string_view PlainArray = "\x1b[96m";
    /** Yellow */
    inline constexpr std::string_view Sized = "\x1b[33m";
    /** Bright yellow */
    inline constexpr std::string_view SizedArray = "\x1b[93m";
    /** Magenta */
    inline constexpr std::string_view Aligned = "\x1b[35m";
    /** Bright magenta */
    inline constexpr std::string_view AlignedArray = "\x1b[95m";
    /** Blue */
    inline constexpr std::string_view SizedAligned = "\x1b[34m";
    /** Bright blue */
    inline constexpr std::string_view SizedAlignedArray = "\x1b[94m";

    /* Alignment checks */

    /** Bold green */
    inline constexpr std::string_view Ok = "\x1b[1;32m";
    /** Bold bright red */
    inline constexpr std::string_view Fail = "\x1b[1;91m";
    /** Bright cyan */
    inline constexpr std::string_view Check = "\x1b[96m";
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
};
