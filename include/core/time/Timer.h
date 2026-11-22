#pragma once

#include <cstdint>

/**
 * Frame timing for the main loop in nanoseconds.
 *
 * Two readings, taken one frame apart. Whatever drives the loop owns one of these and asks it
 * for the delta each frame.
 */
class Timer
{
public:
    Timer();

    /* One clock per loop, and nothing should be able to quietly copy it out from under one */
    Timer(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer& operator=(Timer&&) = delete;

    /**
     * Sets both readings to now. Called once, just before the loop begins.
     */
    void Start();

    /**
     * Takes a new reading and keeps the last one. Called once per frame.
     */
    void UpdateFrameTicks();

    /**
     * How long the last frame took.
     * @return seconds between the two readings
     */
    float GetDeltaTime() const;

    /** @return nanoseconds between the timer's last two readings */
    std::uint64_t GetDeltaNanoseconds() const;

private:
    std::uint64_t previousTicks;
    std::uint64_t currentTicks;
};
