#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/** Adds one draw item per entity that has a Position, a Rotation, and a Renderable. Runs in Extract */
class DrawRenderablesSystem : public System
{
public:
    /** @param world the world whose drawable entities this system reads */
    explicit DrawRenderablesSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Every entity with a Position, a Rotation, and a Renderable */
    Query drawables;
};
