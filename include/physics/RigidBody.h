#pragma once
#include "Quaternion.h"
#include "Vector.h"

struct Transform;

namespace PHYSICS
{
    struct BoxCollider;
    struct SphereCollider;
    struct CylinderCollider;
    struct CapsuleCollider;
    struct MassProperties;

    /**
     * Applies forces such as gravity and drag to a given velocity and returns the resulting velocity.
     * @param velocity The current velocity of the object as a 3D vector.
     * @param mass The mass of the object. Must be greater than a very small threshold to avoid division by zero.
     * @param deltaTime The time step over which the forces are applied.
     * @return The updated velocity after applying the forces over the given time step.
     */
    [[nodiscard]] MATH::Vec3 ApplyForces(const MATH::Vec3& velocity, float mass, float deltaTime);

    /**
     * Updates the position of an object based on its current position, velocity, and the time step.
     * @param position The current position of the object as a 3D vector.
     * @param velocity The current velocity of the object as a 3D vector.
     * @param deltaTime The time step over which the position is updated. Must be a non-negative float.
     * @return The updated position of the object after applying the velocity over the given time step.
     */
    [[nodiscard]] MATH::Vec3 UpdatePosition(const MATH::Vec3& position, const MATH::Vec3& velocity, float deltaTime);

    /**
     * Updates the orientation of an object based on its current orientation, angular velocity, and the time step.
     * @param orientation The current orientation of the object represented as a quaternion.
     * @param angularVelocity The angular velocity of the object as a 3D vector, representing the axis of rotation and its magnitude.
     * @param deltaTime The time step over which the angular velocity is applied.
     * @return The updated orientation of the object after applying the angular velocity over the given time step.
     */
    [[nodiscard]] MATH::Quaternion UpdateOrientation(const MATH::Quaternion& orientation,
                                                     const MATH::Vec3& angularVelocity, float deltaTime);

    /**
     * Applies The force of drag for angular rotation for an object
     * @param angularVelocity current angular velocity of the object to apply drag to
     * @param deltaTime The time step over which the forces are applied.
     * @return The updated velocity after applying the drag force over the given time step.
     */
    [[nodiscard]] MATH::Vec3 ApplyAngularDrag(const MATH::Vec3& angularVelocity, float deltaTime);

    /**
     * Applies the "grip" damping on angular rotation, a modifier meant to control rotation while an object is held
     * @param angularVelocity current angular velocity of the object to apply grip to
     * @param deltaTime The time step over which the forces are applied.
     * @return The updated velocity after applying the grip force over the given time step.
     */
    [[nodiscard]] MATH::Vec3 ApplyGrip(const MATH::Vec3& angularVelocity, float deltaTime);

    /**
     * Limits the angular speed of an object to a certain limit
     * @param angularVelocity  current angular velocity of the object to apply the limit
     * @return The limited angular velocity of the object.
     */
    [[nodiscard]] MATH::Vec3 LimitSpin(const MATH::Vec3& angularVelocity);

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

    /**
    * How hard a shape is to turn about each of its own axes, in kg m^2
    * @param box the shape, at scale 1
    * @param mass the body's mass, in kilograms
    * @param scale the transform's uniform scale
    * @return a diagonal inertia tensor in the shape's own space
    */
    MATH::Matrix3 InertiaTensor(const BoxCollider& box, float mass, float scale);
    /**
    * How hard a shape is to turn about each of its own axes, in kg m^2
    * @param sphere the shape, at scale 1
    * @param mass the body's mass, in kilograms
    * @param scale the transform's uniform scale
    * @return a diagonal inertia tensor in the shape's own space
    */
    MATH::Matrix3 InertiaTensor(const SphereCollider& sphere, float mass, float scale);

    /**
    * How hard a shape is to turn about each of its own axes, in kg m^2
    * @param capsule the shape, at scale 1
    * @param mass the body's mass, in kilograms
    * @param scale the transform's uniform scale
    * @return a diagonal inertia tensor in the shape's own space
    */
    MATH::Matrix3 InertiaTensor(const CapsuleCollider& capsule, float mass, float scale);

    /**
    * How hard a shape is to turn about each of its own axes, in kg m^2
    * @param cylinder the shape, at scale 1
    * @param mass the body's mass, in kilograms
    * @param scale the transform's uniform scale
    * @return a diagonal inertia tensor in the shape's own space
    */
    MATH::Matrix3 InertiaTensor(const CylinderCollider& cylinder, float mass, float scale);
}
