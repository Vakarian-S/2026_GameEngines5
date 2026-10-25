#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/** TEST SYSTEM - Moves every entity by its velocity. One that leaves the box comes back in through the opposite wall */
class MovementSystem : public System
{
public:
    /**
     * @param world the world whose moving entities this system moves
     * @param boxHalfWidth_ half the width of the box the entities stay inside, in metres
     */
    MovementSystem(World& world, float boxHalfWidth_);

    void Run(const SystemContext& context) override;

private:
    /** Every entity with a Position and a Velocity */
    Query movers;
    /** Half the width of the box the entities stay inside, in metres */
    float boxHalfWidth;
};