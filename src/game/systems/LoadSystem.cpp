#include "game/systems/LoadSystem.h"

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_timer.h>

LoadSystem::LoadSystem() : System("Load", 0, 0)
{
}

void LoadSystem::SetLoad(const double milliseconds_, const uint32_t spikeEvery_)
{
    milliseconds = milliseconds_;
    spikeEvery = spikeEvery_;
    frameCount = 0;
}

void LoadSystem::Run(const SystemContext&)
{
    ++frameCount;
    if (milliseconds <= 0.0) return;
    if (spikeEvery != 0 && frameCount % spikeEvery != 0) return;

    const uint64_t end = SDL_GetTicksNS() + static_cast<uint64_t>(milliseconds * 1e6);
    /* Spins rather than sleeps, so the CPU works the way real game code would */
    while (SDL_GetTicksNS() < end)
    {
        SDL_CPUPauseInstruction();
    }
}