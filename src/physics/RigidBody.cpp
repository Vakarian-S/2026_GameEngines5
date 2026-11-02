#include "physics/RigidBody.h"

#include <cassert>

#include "MMath.h"
#include "QMath.h"
#include "core/Components.h"

namespace
{
    constexpr float kGravity = -9.8f;
    constexpr float kDragCoefficient = 0.25f;
}

MATH::Vec3 PHYSICS::ApplyForces(const MATH::Vec3& velocity, const float mass, const float deltaTime)
{
    assert(mass > MATH::kVerySmall && "Trying to apply force to mass-less object.");
    const auto gravity = MATH::Vec3(0.0f, kGravity * mass, 0.0f);
    const MATH::Vec3 drag = velocity * (-kDragCoefficient);
    const MATH::Vec3 force = gravity + drag;
    const MATH::Vec3 acceleration = force / mass;
    return velocity + acceleration * deltaTime;
}

MATH::Vec3 PHYSICS::UpdatePosition(const MATH::Vec3& position, const MATH::Vec3& velocity, const float deltaTime)
{
    return position + velocity * deltaTime;
}

MATH::Quaternion PHYSICS::UpdateOrientation(const MATH::Quaternion& orientation, const MATH::Vec3& angularVelocity,
                                            const float deltaTime)
{
    if (MATH::VMath::mag(angularVelocity) < MATH::kVerySmall) return orientation;
    const float angle = MATH::VMath::mag(angularVelocity) * deltaTime * MATH::kRadiansToDegrees;
    const MATH::Vec3 axis = MATH::VMath::normalize(angularVelocity);
    return MATH::QMath::normalize(MATH::QMath::angleAxisRotation(angle, axis) * orientation);
}

PHYSICS::VelocityChange PHYSICS::MouseConstraint(const Transform& transform,
                                                 [[maybe_unused]] const MATH::Vec3& localPoint,
                                                 const MATH::Vec3& mouseTarget,
                                                 [[maybe_unused]] const MassProperties& massProperties,
                                                 const MATH::Vec3& velocity,
                                                 const MATH::Vec3& angularVelocity, [[maybe_unused]] float deltaTime)
{
    const MATH::Vec3 r = mouseTarget - transform.position;
    const MATH::Matrix3 mEff{
        1 + r.z * r.z + r.y * r.y, -r.x * r.y, -r.x * r.z,
        -r.y * r.x, 1 + r.x * r.x + r.z * r.z, -r.y * r.z,
        -r.x * r.z, -r.y * r.z, 1 + r.x * r.x + r.y * r.y
    };
    const MATH::Vec3 jv = velocity + MATH::VMath::cross(angularVelocity, r);
    const MATH::Vec3 lambda = MATH::MMath::inverse(mEff) * -jv;
    const MATH::Vec3 changeLinearVelocity = lambda;
    const MATH::Vec3 changeAngularVelocity = MATH::VMath::cross(r, lambda);
    return {.changeInLinearVel = changeLinearVelocity, .changeInAngularVel = changeAngularVelocity};
}
