#pragma once
#include <cstdint>

/**
 * Holds the loop to a frame rate. Each frame gets a fixed budget, and Wait sleeps until that
 * frame's deadline, then spins the last part, because a sleep can wake late.
 */
class FrameCap
{
public:
    FrameCap();

    /**
     * Sets the frame rate to hold, and starts the deadlines again on the next Wait.
     * @param framesPerSecond_ the target rate, or 0 for no cap
     */
    void SetFramesPerSecond(double framesPerSecond_);

    /** @return the frame cap's target rate, or 0 when there is no cap */
    double GetFramesPerSecond() const { return framesPerSecond; }

    /**
     * Waits until this frame's deadline, then moves the deadline on by one budget. Returns at once
     * with no cap. A frame more than one budget late starts the deadlines again from now.
     */
    void Wait();

private:
    /** The frame cap's target rate, or 0 for no cap */
    double framesPerSecond;
    /** Nanoseconds per frame at the cap, or 0 for no cap */
    std::uint64_t budgetNanoseconds;
    /** When the current frame should end, in SDL nanosecond ticks. 0 before the first Wait */
    std::uint64_t frameEndNanoseconds;
};
