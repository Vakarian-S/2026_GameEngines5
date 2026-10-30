#pragma once
#include "Vector.h"

struct Camera;
struct Transform;

namespace PHYSICS
{
    /**
     * A mathematical Ray that contains an origin and a Direction
     */
    struct Ray
    {
        /** At what point does the ray start */
        MATH::Vec3 origin;
        /** Direction vector of the ray. Its length sets the units of t, and any length works. */
        MATH::Vec3 direction;

        /**
         * Gets the position of a point at t along the ray
         * @param t how many direction lengths to walk along the ray
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
        /** How many direction lengths along the ray the hit is. With a pick ray this is metres from the camera. */
        float t;
    };

    /**
     * Transforms a Ray from World Space to a shape's local space based on its transform
     * @param worldRay Ray in world space to transform to local space
     * @param transform Transform of the target shape to be used to make the ray in local space
     * @return Ray instance in local space relative to the transform
     */
    Ray ToLocalRay(const Ray& worldRay, const Transform& transform);

    /**
     * Turns a mouse position into a world-space Ray by running the Camera view pipeline backwards
     * @param mousePixel Position of the mouse in window units, with y pointing down
     * @param windowSize Current Window size
     * @param camera Camera being used for the translation
     * @return Ray in world space with a unit length direction, so its t is in metres
     */
    Ray MakePickRay(const MATH::Vec2& mousePixel, const MATH::Vec2& windowSize, const Camera& camera);
}
