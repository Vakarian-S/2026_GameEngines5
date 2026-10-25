#pragma once

/**
 * Time step manager class, these class ensures to accumulate the time between the frames and count whenever 
 * we have taken the time required for a fixed step
 */
class FixedStep
{
public:
    /** How much time it takes for us to consider a fixed time step */
    static constexpr float kFixedTimeStep = 1.0f / 60.0f;

    /**
     * Adds the time elapsed in a frame to our accumulator
     * @param frameTime time since last frame
     */
    void Add(float frameTime);

    /**
     * Checks whether there is enough time in the accumulator to take a fixed step, subtracting it from the accumulator
     * if we did
     * @return boolean indicating whether a step has taken place yet
     */
    bool TakeStep();

    /**
     * Resets the current accumulator
     */
    void Reset();

private:
    /** Limit the max amount of time accumulated between frames, so a big hitch doesn't queue up a bunch of frames */
    static constexpr float kMaxAccumulated = 0.25f;
    /** Accumulator of time between frames */
    float accumulator = 0.0f;
};
