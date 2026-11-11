#include "core/FrameCap.h"

#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_atomic.h>

namespace
{
    /** How long FrameCap spins at the end of a wait. A Windows sleep can wake up to about a millisecond late */
    constexpr std::uint64_t kSpinNanoseconds = 1'000'000;
}

FrameCap::FrameCap() : framesPerSecond(0.0), budgetNanoseconds(0), frameEndNanoseconds(0)
{
}

void FrameCap::SetFramesPerSecond(const double framesPerSecond_)
{
    framesPerSecond = framesPerSecond_;
    budgetNanoseconds = framesPerSecond > 0.0 ? static_cast<std::uint64_t>(1e9 / framesPerSecond) : 0;
    frameEndNanoseconds = 0;
}

void FrameCap::Wait()
{
    if (budgetNanoseconds == 0) return;

    const std::uint64_t now = SDL_GetTicksNS();
    /* The first frame, or a frame more than one budget late, starts the deadlines again from now */
    if (frameEndNanoseconds == 0 || now > frameEndNanoseconds + budgetNanoseconds) frameEndNanoseconds = now;

    if (frameEndNanoseconds > now)
    {
        const std::uint64_t remaining = frameEndNanoseconds - now;
        if (remaining > kSpinNanoseconds) SDL_DelayNS(remaining - kSpinNanoseconds);
        /* Spins until the deadline, because the sleep can wake late */
        while (SDL_GetTicksNS() < frameEndNanoseconds)
        {
            /* Tells the CPU this is a wait loop, so the core uses less power */
            SDL_CPUPauseInstruction();
        }
    }
    frameEndNanoseconds += budgetNanoseconds;
}
