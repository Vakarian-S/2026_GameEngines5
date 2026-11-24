#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/** Adds a wire shape for each entity with a TransformComponent and a collider */
class DrawCollidersSystem : public System
{
public:
    /** @param world the world whose drawable entities this system reads */
    explicit DrawCollidersSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Every entity with a TransformComponent and a BoxColliderComponent */
    Query boxColliderDrawables;
    /** Every entity with a TransformComponent and a SphereColliderComponent */
    Query sphereColliderDrawables;
    /** Every entity with a TransformComponent and a CylinderColliderComponent */
    Query cylinderColliderDrawables;
    /** Every entity with a TransformComponent and a CapsuleColliderComponent */
    Query capsuleColliderDrawables;
};
