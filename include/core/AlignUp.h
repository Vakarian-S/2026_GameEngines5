#pragma once
#include <cstdint>

/**
 * Gets the aligned bytes for a given value, rounded up
 * @param value The value to be aligned
 * @param alignment The desired alignment, must be a power of two
 * @return value aligned up to the given alignment
 */
inline std::uint64_t AlignUp(const std::uint64_t value, const std::uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}
