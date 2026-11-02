#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * Takes currently picked entities and applies mouse constraints to both its velocities
 * the entities must have the mass properties component
 */
class MouseConstraintSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit MouseConstraintSystem(World& world);
    
    void Run(const SystemContext& context) override;
private:
    /** Entities to apply the constraints to */
    Query entities;
};


