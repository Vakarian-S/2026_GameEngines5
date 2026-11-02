#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * Calculates all physical forces acting on entities that are tagged as "Falling" and updates the velocity accordingly
 */
class ApplyForcesSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit ApplyForcesSystem(World& world);
    
    void Run(const SystemContext& context) override;
private:
    /** Entities to update the velocity of */
    Query entities;
};


