#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * Checks all entities With a damage component and checks them against any component with a health component. Damaging
 * the health component on collision, only works on entities with sphere colliders
 */
class ProjectileHitSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit ProjectileHitSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    Query projectiles;
    Query targets;
};


