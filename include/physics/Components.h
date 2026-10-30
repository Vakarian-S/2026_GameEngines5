#pragma once
#include "Vector.h"
#include "physics/Ray.h"

namespace PHYSICS
{
    /** How far an entity moves each second, in metres. */
    struct Velocity
    {
        MATH::Vec3 metresPerSecond;
    };

    /** Component for Collision in the shape of a sphere */
    struct SphereCollider
    {
        /** Radius of the sphere collider, at scale 1 */
        float radius = 0.5f;
    };

    /** Component for Collision in the shape of a box */
    struct BoxCollider
    {
        /** Half the size of a side of the box measured from the centre */
        MATH::Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    };

    /** Component for Collision in the shape of a cylinder */
    struct CylinderCollider
    {
        /** Radius of the cylinder at scale 1 */
        float radius = 0.5f;
        /** Half-height of the cylinder measured from the centre*/
        float halfHeight = 0.5f;
    };

    /** Component for Collision in the shape of a capsule */
    struct CapsuleCollider
    {
        /** Radius of the cylinder and caps at scale 1 */
        float radius = 0.5f;
        /** Half-height of the straight part of the capsule measured from the centre */
        float halfHeight = 0.5f;
    };

    /**
     * Indicates an entity has been picked, holding the picked point in local space
     */
    struct Picked
    {
        MATH::Vec3 localPoint;
    };

    /**
     * Indicates a ray specifically for picking other entities
     */
    struct PickRay
    {
        Ray ray;
        float t;
        bool hasHit;
    };
}
