#pragma once

#include "core/Handles.h"
#include "MMath.h"
#include "Quaternion.h"
#include "Vector.h"
#include "VMath.h"

/** Transform of an entity, required to build a Model Matrix */
struct Transform
{
    /** Position of the unit in metres */
    MATH::Vec3 position;
    /** Scale along each of the shape's own axes. Round colliders need all three equal */
    MATH::Vec3 scale = MATH::Vec3(1.0f, 1.0f, 1.0f);
    /** Where the entity is currently facing */
    MATH::Quaternion orientation;
};

/**
 * Builds translate × rotate × scale from a Transform, so meshes and wires land in the same place
 * @param transform transform component data
 * @return Model Matrix generated from the transform
 */
inline MATH::Matrix4 ModelMatrix(const Transform& transform)
{
    /* Turns the mesh about its own centre first, then moves it into place. */
    return
        MATH::MMath::translate(transform.position.x, transform.position.y,
                               transform.position.z) *
        MMath::toMatrix4(transform.orientation) * MMath::scale(
            transform.scale.x, transform.scale.y, transform.scale.z);
}

/**
 * Builds the matrix that turns a mesh's normals into world space. Under non-uniform scale the model's
 * plain 3x3 tilts normals, so this is the cofactor matrix: the inverse transpose times the determinant.
 * The shader normalizes normals, so the factor drops out; there is no division, so a tiny scale cannot throw.
 * @param model the model matrix from ModelMatrix
 * @return a 3x3 that keeps normals perpendicular to their surface
 */
inline MATH::Matrix3 NormalMatrix(const MATH::Matrix4& model)
{
    /* The 3x3's columns are where the shape's x, y and z axes land */
    const MATH::Vec3 x(model[0], model[1], model[2]);
    const MATH::Vec3 y(model[4], model[5], model[6]);
    const MATH::Vec3 z(model[8], model[9], model[10]);

    /* Each cofactor column is the cross product of the other two axes */
    MATH::Vec3 xCofactor = MATH::VMath::cross(y, z);
    MATH::Vec3 yCofactor = MATH::VMath::cross(z, x);
    MATH::Vec3 zCofactor = MATH::VMath::cross(x, y);

    /* A mirroring scale makes the determinant negative, which would flip every normal inward */
    if (MATH::VMath::dot(x, xCofactor) < 0.0f)
    {
        xCofactor = -xCofactor;
        yCofactor = -yCofactor;
        zCofactor = -zCofactor;
    }
    /* The Matrix3 constructor takes columns */
    return MATH::Matrix3(
        xCofactor.x, xCofactor.y, xCofactor.z,
        yCofactor.x, yCofactor.y, yCofactor.z,
        zCofactor.x, zCofactor.y, zCofactor.z);
}

/** Marks the entity as renderable */
struct Renderable
{
    MeshHandle meshHandle;
    MaterialHandle materialHandle;
};
