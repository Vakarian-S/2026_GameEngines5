#include "core/FixedStep.h"

#include <algorithm>

void FixedStep::Add(const float frameTime)
{
    accumulator = std::ranges::min(kMaxAccumulated, accumulator + frameTime);
}

bool FixedStep::TakeStep()
{
    if (accumulator < kFixedTimeStep) return false;
    accumulator -= kFixedTimeStep;
    return true;
}

void FixedStep::Reset()
{
    accumulator = 0.0f;
}
