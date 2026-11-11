#include "core/FrameTimeStats.h"

#include <algorithm>

namespace
{
    /** One report's window of frame times: 1 second, in nanoseconds */
    constexpr std::uint64_t kWindowNanoseconds = 1'000'000'000;

    /** Nanoseconds in one millisecond, for the report's millisecond numbers */
    constexpr double kNanosecondsPerMs = 1'000'000.0;
}

bool FrameTimeStats::AddFrame(const std::uint64_t nanoseconds)
{
    ++frameCount;
    totalNanoseconds += nanoseconds;
    shortestNanoseconds = std::min(shortestNanoseconds, nanoseconds);
    longestNanoseconds = std::max(longestNanoseconds, nanoseconds);

    if (totalNanoseconds < kWindowNanoseconds) return false;

    const double totalSeconds = static_cast<double>(totalNanoseconds) / 1e9;
    report.fps = frameCount / totalSeconds;
    report.averageMs = static_cast<double>(totalNanoseconds) / frameCount / kNanosecondsPerMs;
    report.shortestMs = static_cast<double>(shortestNanoseconds) / kNanosecondsPerMs;
    report.longestMs = static_cast<double>(longestNanoseconds) / kNanosecondsPerMs;

    Reset();
    return true;
}

void FrameTimeStats::Reset()
{
    frameCount = 0;
    totalNanoseconds = 0;
    shortestNanoseconds = std::numeric_limits<std::uint64_t>::max();
    longestNanoseconds = 0;
}