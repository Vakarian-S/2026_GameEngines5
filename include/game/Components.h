#pragma once
#include "core/Handles.h"

/** Where an entity sits in the world, in metres. */
struct Position
{
    float x, y, z;
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

/** The rotation of said entity */
struct Rotation
{
    float yawRadians;
};

/** Makes the entity spin */
struct Spin
{
    float radiansPerSecond;
};
