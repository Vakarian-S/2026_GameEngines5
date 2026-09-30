#pragma once

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
