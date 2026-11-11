#pragma once

/** Marks an entity that was hit. Carries nothing, so it is a tag. */
struct Hit
{
};

/** Marks an entity that the damage system tags and untags. Carries nothing, so it is a tag */
struct Damageable
{
};

/** Marks an entity in the pacing scene's top row, which moves once per frame */
struct FrameStepRow
{
};

/** Marks an entity in the pacing scene's bottom row, which moves once per fixed step*/
struct FixedStepRow
{
};