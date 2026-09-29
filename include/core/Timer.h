#pragma once

/**
 * Frame timing for the main loop.
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

    /**
     * How long to wait to hold a frame rate, measured from the last reading to right now.
     * @param fps_ frames per second to aim for
     * @return milliseconds to sleep, or zero when the frame already ran long
     */
    unsigned int GetSleepTime(const unsigned int fps_) const;

    /**
    * Seconds elapsed at the last reading. Constant for the whole frame.
    * @return seconds since Start
    */
    float GetCurrentTicks() const;

private:
    unsigned int previousTicks;
    unsigned int currentTicks;
};
