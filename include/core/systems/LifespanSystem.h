#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/** Queries all entities with a Lifespan components and check if they need to be deleted */
class LifespanSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit LifespanSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Entities with a LifespanComponent */
    Query entities;
};


