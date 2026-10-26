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
