#pragma once
#include "ecs/Component.h"
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/** TEST SYSTEM - Moves one row of entities right at their speed, and wraps each one back to the start of its path */
class SlideRightSystem : public System
{
public:
    /**
     * @param world the world whose row entities this system moves
     * @param name_ the system's name in the profiler
     * @param rowTag the tag that picks the row this system moves
     * @param leftEdge_ the x where the path starts, in metres
     * @param pathWidth_ the path's width in metres. An entity past the right end moves back by this much
     */
    SlideRightSystem(World& world, const char* name_, ComponentMask rowTag, float leftEdge_, float pathWidth_);

    void Run(const SystemContext& context) override;

private:
    /** Every entity in the row, with a Transform and a Velocity */
    Query movers;
    /** The x where the slide path starts, in metres */
    float leftEdge;
    /** The slide path's width, in metres */
    float pathWidth;
};