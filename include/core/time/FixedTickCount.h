#pragma once
#include <cstdint>

/**
 * The fixed steps this World has run. The application adds 1 and sets it before each Fixed run, so the first step reads 1
 */
struct FixedTickCount
{
    std::uint64_t ticks;
};
