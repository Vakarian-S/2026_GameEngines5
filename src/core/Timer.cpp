#include "core/Timer.h"

#include <SDL3/SDL_timer.h>

Timer::Timer() : previousTicks(0), currentTicks(0)
{
}

void Timer::Start()
{
    previousTicks = SDL_GetTicksNS();
    currentTicks = previousTicks;
}

void Timer::UpdateFrameTicks()
{
    previousTicks = currentTicks;
    currentTicks = SDL_GetTicksNS();
}

float Timer::GetDeltaTime() const
{
    const uint64_t delta = currentTicks - previousTicks;
    return static_cast<float>(delta) / 1'000'000'000.0f;
}
