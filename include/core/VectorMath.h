#pragma once
#include "Vector.h"
#include "VMath.h"
#include "ConstantsConversions.h"

/**
 * Normalizes a vector, or returns the fallback when the vector is too short to have a direction.
 * For places where a zero vector is normal, such as no movement key held.
 * @param vector Vector to normalize
 * @param fallback Vector to return in case the normalize cannot be done
 * @return The normalized vector or the fallback if applicable
 */
inline MATH::Vec3 SafeNormalize(const MATH::Vec3& vector, const MATH::Vec3& fallback = MATH::Vec3())
{
    if (MATH::VMath::mag(vector) < VERY_SMALL) return fallback;
    return MATH::VMath::normalize(vector);
}

/**
 * Multiplies two vectors one component at a time: (a.x * b.x, a.y * b.y, a.z * b.z).
 * For applying a per-axis scale to a point in the shape's own space.
 * @param a first vector
 * @param b second vector
 * @return the per-component product
 */
inline MATH::Vec3 MultiplyComponents(const MATH::Vec3& a, const MATH::Vec3& b)
{
    return MATH::Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

/**
 * Divides two vectors one component at a time: (a.x / b.x, a.y / b.y, a.z / b.z).
 * For undoing a per-axis scale. Every component of the divisor must be non-zero.
 * @param a the vector to divide
 * @param b the divisor
 * @return the per-component quotient
 */
inline MATH::Vec3 DivideComponents(const MATH::Vec3& a, const MATH::Vec3& b)
{
    return MATH::Vec3(a.x / b.x, a.y / b.y, a.z / b.z);
}

/**
 * The smallest of a vector's three components.
 * @param vector the vector to read
 * @return min(x, y, z)
 */
inline float MinComponent(const MATH::Vec3& vector)
{
    const float smallerXY = vector.x < vector.y ? vector.x : vector.y;
    return smallerXY < vector.z ? smallerXY : vector.z;
}
