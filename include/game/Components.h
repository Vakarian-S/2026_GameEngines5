#pragma once
#include "Vector.h"
#include "Quaternion.h"
#include "core/Handles.h"

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

/** How far an entity moves each second, in metres. */
struct Velocity
{
    float x, y, z;
};

/** Marks an entity that was hit. Carries nothing, so it is a tag. */
struct Hit
{
};

/** Marks the entity as renderable */
struct Renderable
{
    MeshHandle meshHandle;
    MaterialHandle materialHandle;
};

/** Makes the entity spin */
struct Spin
{
    float radiansPerSecond;
};

/** Marks an entity that the damage system tags and untags. Carries nothing, so it is a tag */
struct Damageable
{
};
