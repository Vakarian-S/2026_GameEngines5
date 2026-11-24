#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * System traps entities considered players via PlayerCommandComponent to limit their movement to the arena of the given dimension
 */
class ClampToArenaSystem : public System
{
public:
    /** @param world the world whose entities this system reads
     * @param halfWidth_ half width of the arena to clamp to
     * @param halfDepth_ half height of the arena to clamp to
     */
    ClampToArenaSystem(World& world, float halfWidth_, float halfDepth_);

    void Run(const SystemContext& context) override;

private:
    Query entities;
    /** Half width of the arena to clamp to, related to the X axis */
    float halfWidth;
    /** Half depth of the arena to clamp to, related to the Z axis */
    float halfDepth;
};


