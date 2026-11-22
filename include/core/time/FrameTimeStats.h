#pragma once
#include <cstdint>
#include <limits>

/** One second of frame times, summed up: the frame rate, the average, the shortest and the longest frame */
struct FrameTimeReport
{
    /** Frames per second over the report's window */
    double fps = 0.0;
    /** The average frame time over the report's window, in milliseconds */
    double averageMs = 0.0;
    /** The shortest frame in the report's window, in milliseconds */
    double shortestMs = 0.0;
    /** The longest frame in the report's window, in milliseconds */
    double longestMs = 0.0;
};

/**
 * Collects frame times and sums them up once a second. It never reads a clock: the loop hands it
 * each frame's time, so a test can hand it fixed times.
 */
class FrameTimeStats
{
public:
    /**
     * Adds one frame. When the frames added since the last report reach 1 second, it builds a new
     * report and starts the next second from zero.
     * @param nanoseconds how long the frame took
     * @return true when this frame finished a report
     */
    bool AddFrame(std::uint64_t nanoseconds);

    /** @return the last finished report of frame times; all zeros before the first second ends */
    const FrameTimeReport& GetReport() const { return report; }

private:
    /** Clears the frame count, total, shortest, and longest, ready for the next second */
    void Reset();

    /** Frames added in the current second */
    std::uint32_t frameCount = 0;
    /** Sum of the frame times in the current second, in nanoseconds */
    std::uint64_t totalNanoseconds = 0;
    /** Shortest frame in the current second, in nanoseconds. Starts at the largest value, so the first frame replaces it */
    std::uint64_t shortestNanoseconds = std::numeric_limits<std::uint64_t>::max();
    /** Longest frame in the current second, in nanoseconds */
    std::uint64_t longestNanoseconds = 0;
    /** The last finished report of frame times */
    FrameTimeReport report;
};
