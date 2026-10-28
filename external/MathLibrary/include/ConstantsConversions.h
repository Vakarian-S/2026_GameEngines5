#ifndef CONTSTANTSCONVERSIONS_H
#define CONTSTANTSCONVERSIONS_H

#include <numbers>

namespace MATH
{
	/** Pi as a float, rounded correctly by the standard library */
	inline constexpr float kPi = std::numbers::pi_v<float>;
	/** Multiply an angle in degrees by this to get radians */
	inline constexpr float kDegreesToRadians = kPi / 180.0f;
	/** Multiply an angle in radians by this to get degrees */
	inline constexpr float kRadiansToDegrees = 180.0f / kPi;
}



    /// This is used in normalizing vectors. Dividing by zero is a well known
	/// problem but dividing by nearly zero is also a problem. 1.0x10-7 is very
	/// small in "float" percision (IEEE 754). 
#ifndef VERY_SMALL
#define VERY_SMALL 1.0e-7f
#endif

#ifndef VERY_CLOSE_TO_ONE 
#define	VERY_CLOSE_TO_ONE 0.9999f
#endif

#endif