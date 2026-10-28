#pragma once

#include "core/Handles.h"
#include "MMath.h"
#include "Quaternion.h"
#include "Vector.h"

/** Transform of an entity, required to build a Model Matrix */
struct Transform
{
    /** Position of the unit in metres */
    MATH::Vec3 position;
    /** Uniform scale to apply on every axis */
    float scale = 1.0f;
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
            transform.scale, transform.scale, transform.scale);
}

/** Marks the entity as renderable */
struct Renderable
{
    MeshHandle meshHandle;
    MaterialHandle materialHandle;
};
