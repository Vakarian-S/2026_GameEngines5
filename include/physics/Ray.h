#pragma once
#include "Vector.h"

namespace PHYSICS
{
    /**
     * A mathematical Ray that contains an origin and a Direction
     */
    struct Ray
    {
        /** At what point does the ray start */
        MATH::Vec3 origin;
        /** Direction vector of the ray, not normalized so t compares across shapes. */
        MATH::Vec3 direction;

        /**
         * Gets the position of a point at t along the ray
         * @param t distance along the ray to find
         * @return point along the ray where t sits
         */
        [[nodiscard]] MATH::Vec3 PointAt(const float t) const { return origin + direction * t; }
    };

    /**
     * Struct representing a hit of a ray and a given target, storing the intersection position and at what point
     * along the ray was the intersection at.
     */
    struct RayHit
    {
        /** Where the ray hit in LOCAL space */
        MATH::Vec3 intersectionPosition;
        /** At what distance along the ray the intersection happened */
        float t;
    };
}
