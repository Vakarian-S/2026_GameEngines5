#pragma once

#include "Vector.h"
#include "core/Components.h"

#include <cstdint>

/** Marks an entity that was hit. Carries nothing, so it is a tag. */
struct HitTag
{
};

/** Marks an entity that the damage system tags and untags. Carries nothing, so it is a tag */
struct DamageableTag
{
};

/** Marks an entity in the pacing scene's top row, which moves once per frame */
struct FrameStepRowTag
{
};

/** Marks an entity in the pacing scene's bottom row, which moves once per fixed step*/
struct FixedStepRowTag
{
};

/** Player commands inputted by the player */
struct PlayerCommandComponent
{
    MATH::Vec3 moveDirection;
    /** The direction for shooting */
    MATH::Vec3 aimDirection;
};

/** Tag indicating which one is the currently owned player */
struct LocalPlayerTag
{
};

/** Indicates this is the boss entity of the boss fight scene */
struct BossTag
{
};

/** A figure-eight path on the floor that an entity follows, one loop every periodTicks */
struct FigureEightPathComponent
{
    /** World position where the path crosses itself */
    MATH::Vec3 center;
    /** Ticks for one full loop of the path */
    std::uint64_t periodTicks;
    /** Distance from the center to the path's far left and right, along x */
    float halfWidth;
    /** Distance from the center to the path's far front and back, along z */
    float halfDepth;
};

/** Give the player a gun feature with cooldown between shots*/
struct GunComponent
{
    /** At what tick the gun can shoot again */
    std::uint64_t readyTick;
    /** Speed of the projectile shot */
    float projectileSpeed;
    /** The lifespan tick of the projectiles shot by this gun*/
    std::uint64_t lifetimeTicks;
    /** Cooldown ticks of this gun*/
    std::uint64_t cooldownTicks;
    /** Damage the projectile deals */
    std::int32_t projectileDamage;
    /** Size of the projectile shot on every axis. It scales the mesh and the sphere collider together */
    float projectileScale = 1.0f;
    /** The renderable for the mesh */
    RenderableComponent projectileRenderable;
    
};

/** Component that indicates this entity can be damaged and destroyed */
struct HealthComponent
{
    std::int32_t points;
};

/** Component that indicates this entity damages entities with health components*/
struct DamageComponent
{
    std::int32_t damage;
};
