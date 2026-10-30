#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/** Adds a line along the last pick ray, and a cross where it hit */
class DrawPickRaySystem : public System
{
public:
    /** @param world the world whose pick ray this system reads */
    explicit DrawPickRaySystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Every entity with a PickRay Component */
    Query pickRays;
};
