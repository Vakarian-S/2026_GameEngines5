#pragma once
#include <cstdint>

#include "ecs/System.h"

/** TEST SYSTEM - Keeps the CPU busy for a set time on chosen frames, to stand in for game work */
class LoadSystem : public System
{
public:
    LoadSystem();

    /**
     * Sets the work a loaded frame does, and starts counting frames again.
     * @param milliseconds_ how long a loaded frame spins, or 0 for no load
     * @param spikeEvery_ loads one frame in this many, or 0 to load every frame
     */
    void SetLoad(double milliseconds_, uint32_t spikeEvery_);

    void Run(const SystemContext& context) override;

private:
    /** How long a loaded frame spins, in milliseconds. 0 means no load */
    double milliseconds = 0.0;
    /** The load system loads one frame in this many. 0 means every frame */
    uint32_t spikeEvery = 0;
    /** Frames the load system has run since the last SetLoad */
    uint64_t frameCount = 0;
};