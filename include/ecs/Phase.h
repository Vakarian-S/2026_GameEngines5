#pragma once
#include <cstddef>
#include <cstdint>

/** When a system runs in the frame. The order here is the run order. */
enum class Phase : uint8_t
{
	/** Zero or more times per frame, with a fixed step of 1/60 s */
	Fixed,
	/** Once per frame, with the frame's time */
	Update,
	/** Once per frame. Reads the world and fills the draw list and the debug lines */
	Extract,
	/** Not a phase. Always last, so it equals the number of phases above it */
	Count
};

/** How many phases exist. Sizes the registry's list per phase */
constexpr std::size_t kPhaseCount = static_cast<std::size_t>(Phase::Count);
