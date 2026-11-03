#include "physics/RigidBody.h"

#include <cassert>

#include "MMath.h"
#include "QMath.h"
#include "core/Components.h"
#include "physics/Components.h"

namespace
{
    constexpr float kGravity = -9.8f;
    constexpr float kDragCoefficient = 0.25f;
    constexpr float kAngularDrag = 0.25f;
    constexpr float kBeta = 0.15f;
    constexpr float kGripDamping = 7.0f;
    constexpr float kMaxAngularVelocity = 12.0f;

    /**
    * Builds a tensor with only the three axis values filled in
    * @return a diagonal Matrix3
    */
    MATH::Matrix3 Diagonal(const float x, const float y, const float z)
    {
        /* The default constructor is the identity, so the off-diagonals are already zero */
        MATH::Matrix3 tensor;
        tensor[0] = x;
        tensor[4] = y;
        tensor[8] = z;
        return tensor;
    }
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

MATH::Vec3 PHYSICS::ApplyAngularDrag(const MATH::Vec3& angularVelocity, float deltaTime)
{
    return angularVelocity * (1 - kAngularDrag * deltaTime);
}

MATH::Vec3 PHYSICS::ApplyGrip(const MATH::Vec3& angularVelocity, const float deltaTime)
{
    return angularVelocity / (1 + kGripDamping * deltaTime);
}

MATH::Vec3 PHYSICS::LimitSpin(const MATH::Vec3& angularVelocity)
{
    const float currentMag = MATH::VMath::mag(angularVelocity);
    return currentMag > kMaxAngularVelocity
               ? MATH::VMath::normalize(angularVelocity) * kMaxAngularVelocity
               : angularVelocity;
}

PHYSICS::VelocityChange PHYSICS::MouseConstraint(const Transform& transform,
                                                 const MATH::Vec3& localPoint,
                                                 const MATH::Vec3& mouseTarget,
                                                 const MassProperties& massProperties,
                                                 const MATH::Vec3& velocity,
                                                 const MATH::Vec3& angularVelocity,
                                                 const float deltaTime)
{
    /* Some Reference https://github.com/erincatto/box2d/blob/v2.4.1/src/dynamics/b2_mouse_joint.cpp */
    const float mass = massProperties.mass;
    /* The arm: from the centre to the clicked point, turned with the body */
    const MATH::Vec3 r = MATH::QMath::rotate(localPoint * transform.scale, transform.orientation);

    /* The tensor is stored in the shape's own axes; R I^-1 R^T turns it into world space */
    const MATH::Matrix3 rotation = MATH::MMath::toMatrix3(transform.orientation);
        const MATH::Matrix3 inverseInertia = rotation * MATH::MMath::inverse(massProperties.inertiaTensor) *
        MATH::MMath::transpose(rotation);

    /* [r]x, so that cross * v is r x v. The constructor takes columns */
    const MATH::Matrix3 cross(
        0.0f, r.z, -r.y,
        -r.z, 0.0f, r.x,
        r.y, -r.x, 0.0f);

    /* Meff = J M^-1 J^T. J's left half is the identity and its right half is -[r]x,
    * so the product splits into a sliding part and a turning part */

    /* Turning part: [r]x I^-1 [r]x^T. The two minus signs from -[r]x cancel */
    MATH::Matrix3 mEff = cross * inverseInertia * MATH::MMath::transpose(cross);

    /* Sliding part: (1/m) times the identity, which only touches the diagonal */
    mEff[0] += 1.0f / mass;
    mEff[4] += 1.0f / mass;
    mEff[8] += 1.0f / mass;

    const MATH::Vec3 jv = velocity + MATH::VMath::cross(angularVelocity, r);
    const MATH::Vec3 gap = transform.position + r - mouseTarget;
    const MATH::Vec3 bias = gap * (kBeta / deltaTime);
    const MATH::Vec3 lambda = MATH::MMath::inverse(mEff) * (-jv - bias);

    return {
        .changeInLinearVel = lambda * (1.0f / mass),
        .changeInAngularVel = inverseInertia * MATH::VMath::cross(r, lambda)
    };
}

MATH::Matrix3 PHYSICS::InertiaTensor(const BoxCollider& box, const float mass, const float scale)
{
    /* 1/12 m (h² + d²) per axis, with full sizes */
    /* m/3 (b² + c²), since each full size is twice the half: (2b)²/12 = b²/3 */
    const MATH::Vec3 half = box.halfExtents * scale;
    const float x2 = half.x * half.x;
    const float y2 = half.y * half.y;
    const float z2 = half.z * half.z;
    return Diagonal(mass / 3.0f * (y2 + z2), mass / 3.0f * (x2 + z2), mass / 3.0f * (x2 + y2));
}

MATH::Matrix3 PHYSICS::InertiaTensor(const SphereCollider& sphere, const float mass, const float scale)
{
    const float r = sphere.radius * scale;
    const float value = 0.4f * mass * r * r;
    return Diagonal(value, value, value);
}

MATH::Matrix3 PHYSICS::InertiaTensor(const CapsuleCollider& capsule, const float mass, const float scale)
{
    /* Read more: https://gamedev.net/tutorials/programming/math-and-physics/capsule-inertia-tensor-r3856 */
    const float r = capsule.radius * scale;
    const float h = capsule.halfHeight * scale;

    /* Volumes without pi, since it cancels in the split */
    const float tubeVolume = 2.0f * h * r * r;
    const float capsVolume = 4.0f / 3.0f * r * r * r;
    const float tubeMass = mass * tubeVolume / (tubeVolume + capsVolume);
    const float capsMass = mass - tubeMass;

    /* Each cap sits h past the centre, so moving it out adds mass times distance squared */
    const float across = tubeMass * (r * r / 4.0f + h * h / 3.0f)
        + capsMass * (0.4f * r * r + h * h + 0.75f * h * r);
    const float along = 0.5f * tubeMass * r * r + 0.4f * capsMass * r * r;
    return Diagonal(across, along, across);
}

MATH::Matrix3 PHYSICS::InertiaTensor(const CylinderCollider& cylinder, const float mass, const float scale)
{
    /* Wikipedia: 1/12 m (3r² + h²), with h the full height
     * (3r² + 4h²) / 12, since our h is half: (2h)² = 4h² */
    const float r = cylinder.radius * scale;
    const float h = cylinder.halfHeight * scale;
    const float across = mass * (3.0f * r * r + 4.0f * h * h) / 12.0f;
    return Diagonal(across, 0.5f * mass * r * r, across);
}
