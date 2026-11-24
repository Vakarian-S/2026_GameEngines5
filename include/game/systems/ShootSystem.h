#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;


/**
 * Fires a projectile from each entity with a GunComponent along its PlayerCommandComponent's aim while the aim
 * is not zero, at most once every cooldownTicks
 */
class ShootSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit ShootSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Entities with a TransformComponent, a GunComponent and a PlayerCommandComponent */
    Query entities;
};

