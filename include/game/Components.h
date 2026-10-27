#pragma once

/** Marks an entity that was hit. Carries nothing, so it is a tag. */
struct Hit
{
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
