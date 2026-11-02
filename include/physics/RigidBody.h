#pragma once
#include "Quaternion.h"
#include "Vector.h"

struct Transform;

namespace PHYSICS
{
    struct MassProperties;
    
    [[nodiscard]] MATH::Vec3 ApplyForces(const MATH::Vec3& velocity, float mass, float deltaTime);
    [[nodiscard]] MATH::Vec3 UpdatePosition(const MATH::Vec3& position, const MATH::Vec3& velocity, float deltaTime);
    [[nodiscard]] MATH::Quaternion UpdateOrientation(const MATH::Quaternion& orientation,
                                       const MATH::Vec3& angularVelocity, float deltaTime);

    /**
     * Two values indicating the changes in Velocity both angular and linear
     */
    struct VelocityChange
    {
        MATH::Vec3 changeInLinearVel;
        MATH::Vec3 changeInAngularVel;
    };

    [[nodiscard]] VelocityChange MouseConstraint(
        const Transform& transform,
        const MATH::Vec3& localPoint,
        const MATH::Vec3& mouseTarget,
        const MassProperties& massProperties,
        const MATH::Vec3& velocity,
        const MATH::Vec3& angularVelocity,
        float deltaTime
    );
}
