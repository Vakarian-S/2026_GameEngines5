#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * Reads the Angular velocities of entities with that component and updates their orientation
 */
class UpdateOrientationSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit UpdateOrientationSystem(World& world);
    
    void Run(const SystemContext& context) override;
private:
    /** Entities to have their orientations updated */
    Query entities;
};


