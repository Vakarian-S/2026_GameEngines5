#include "game/FigureEightPath.h"

#include <cassert>
#include <cmath>

#include "ConstantsConversions.h"



MATH::Vec3 FigureEightPathPosition(const std::uint64_t tick, const std::uint64_t periodTicks, const float halfWidth,
                                   const float halfDepth)
{
    assert(periodTicks > MATH::kVerySmall && "A period tick of 0 causes division by 0");
    const float angle = 2.f * MATH::kPi * static_cast<float>(tick % periodTicks) / static_cast<float>(periodTicks);
    return {halfWidth * std::sin(angle), 0.f, halfDepth * std::sin(2.f * angle)};
}
