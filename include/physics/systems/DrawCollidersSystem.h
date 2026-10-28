#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/** Adds a wire shape for each entity with a Transform and a collider */
class DrawCollidersSystem : public System
{
public:
    /** @param world the world whose drawable entities this system reads */
    explicit DrawCollidersSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Every entity with a Transform and a Box Collider Component */
    Query boxColliderDrawables;
    /** Every entity with a Transform and a Sphere Collider Component */
    Query sphereColliderDrawables;
    /** Every entity with a Transform and a Cylinder Collider Component */
    Query cylinderColliderDrawables;
    /** Every entity with a Transform and a Capsule Collider Component */
    Query capsuleColliderDrawables;
};
