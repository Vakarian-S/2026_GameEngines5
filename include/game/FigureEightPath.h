#pragma once
#include "Vector.h"

#include <cstdint>

/**
 * Returns a position inside a Figure eight pattern relative to a tick inside a given period
 * @param tick current tick to base the position on
 * @param periodTicks How many ticks are there in a full loop
 * @param halfWidth half width of the figure eight path
 * @param halfDepth half depth of the figure eight path
 * @return Position vector of a point inside the path on a given tick
 */
MATH::Vec3 FigureEightPathPosition(std::uint64_t tick, std::uint64_t periodTicks, float halfWidth, float halfDepth);
