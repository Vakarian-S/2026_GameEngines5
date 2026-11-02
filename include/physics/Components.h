#pragma once
#include "Vector.h"
#include "Matrix.h"
#include "physics/Ray.h"

namespace PHYSICS
{
    /** How far an entity moves each second, in metres. */
    struct Velocity
    {
        MATH::Vec3 metresPerSecond;
    };
    
    /** How fast an entity turns. The direction is the axis, and the length is the speed */
    struct AngularVelocity
    {
        MATH::Vec3 radiansPerSecond;
    };
    
    /** Mass and inertia tensor of a body. */
    struct MassProperties
    {
        float mass = 1.0f;
        /** Matrix for the inertia tensor, default value is the identity */
        MATH::Matrix3 inertiaTensor;
    };
    
    /** Tag indicating that this object can be affected by gravity and drag */
    struct Falling {};

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
        /** Where in the local space was the entity picked */
        MATH::Vec3 localPoint;
        /** T value of the mouse ray indicating at what point along the ray where the component was picked */
        float grabT = 0.0f;
        /** The world point in 3d on where the mouse is sitting when picking */
        MATH::Vec3 mouseTarget;
        
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
