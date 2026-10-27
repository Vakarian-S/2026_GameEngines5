#pragma once

#include "core/Handles.h"
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

/** Marks the entity as renderable */
struct Renderable
{
    MeshHandle meshHandle;
    MaterialHandle materialHandle;
};
