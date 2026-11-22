#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * Moves each entity with a FigureEightPath along its own figure-eight, based on the current tick.
 */
class FigureEightPathSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit FigureEightPathSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Entities with a Transform and a FigureEightPath component */
    Query entities;
};
