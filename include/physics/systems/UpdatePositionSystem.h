#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * Updates the position of all entities with a transform and a velocity
 */
class UpdatePositionSystem : public System
{
public:
    /** @param world the world whose moving entities this system reads */
    explicit UpdatePositionSystem(World& world);
    
    void Run(const SystemContext& context) override;
private:
    /** Every entity with a Transform and a Velocity */
    Query movers;
};
