#include "core/Timer.h"

#include <SDL3/SDL_timer.h>

Timer::Timer() : previousTicks(0), currentTicks(0)
{
}

void Timer::Start()
{
    previousTicks = static_cast<unsigned int>(SDL_GetTicks());
    currentTicks = previousTicks;
}

void Timer::UpdateFrameTicks()
{
    previousTicks = currentTicks;
    currentTicks = static_cast<unsigned int>(SDL_GetTicks());
}

float Timer::GetDeltaTime() const
{
    return static_cast<float>(currentTicks - previousTicks) / 1000.0f;
}

unsigned int Timer::GetSleepTime(const unsigned int fps_) const
{
    if (fps_ == 0) return 0;
    const unsigned int milliSecsPerFrame = 1000 / fps_;
    if (milliSecsPerFrame == 0)
    {
        return 0;
    }

    const unsigned int elapsed = static_cast<unsigned int>(SDL_GetTicks() - currentTicks);
    if (elapsed >= milliSecsPerFrame) return 0;
    return milliSecsPerFrame - elapsed;
}

float Timer::GetCurrentTicks() const
{
    return static_cast<float>(currentTicks) / 1000.0f;
}
