#pragma once
#include <cstdint>

/**
 * Builds the mask that keeps only the timestamp bits a GPU queue writes.
 * @param validBits how many low bits of a timestamp are real, from 0 to 64
 * @return validBits ones in the low bits; all 64 ones when validBits is 64
 */
inline std::uint64_t TimestampMask(const std::uint32_t validBits)
{
    /* A shift by 64 is undefined, so all 64 bits get their own case. */
    /* 1ull << validBits puts a single 1 just above the real bits. Subtracting 1 turns every bit below it into a 1. For 4 valid bits:
    1 << 4 = 10000
    minus 1 = 01111 */
    return validBits >= 64 ? ~0ull : (1ull << validBits) - 1;
}

/**
 * Turns a start and an end timestamp into milliseconds.
 * @param start the timestamp written before the commands
 * @param end the timestamp written after the commands
 * @param mask the mask from TimestampMask, so a counter that wraps between the two still reads right
 * @param period nanoseconds per timestamp tick
 * @return the time between the two timestamps, in milliseconds
 */
inline double TimestampMilliseconds(const std::uint64_t start, const std::uint64_t end, const std::uint64_t mask,
                                    const float period)
{
    /* Masking the difference keeps it right when the counter wraps around. */
    const std::uint64_t elapsed = (end - start) & mask;
    return static_cast<double>(elapsed) * period / 1'000'000.0;
}
