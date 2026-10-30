#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

class PickSystem : public System
{
public:
    /** @param world the world whose pickable entities this system reads */
    explicit PickSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Every entity with a Transform and a Box Collider Component */
    Query boxColliderEntities;
    /** Every entity with a Transform and a Sphere Collider Component */
    Query sphereColliderEntities;
    /** Every entity with a Transform and a Cylinder Collider Component */
    Query cylinderColliderEntities;
    /** Every entity with a Transform and a Capsule Collider Component */
    Query capsuleColliderEntities;
    /** Every entity that was already picked */
    Query pickedEntities;
    /** The Pick Rays drawn */
    Query pickRays;
};
