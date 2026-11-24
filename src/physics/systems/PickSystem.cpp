#include "physics/systems/PickSystem.h"

#include <SDL3/SDL_mouse.h>
#include <limits>
#include <iostream>

#include "core/Camera.h"
#include "core/Components.h"
#include "core/InputState.h"
#include "core/VectorMath.h"
#include "ecs/Archetype.h"
#include "ecs/CommandBuffer.h"
#include "ecs/SystemContext.h"
#include "ecs/World.h"
#include "physics/Components.h"
#include "physics/Ray.h"
#include "physics/RayIntersect.h"


PickSystem::PickSystem(World& world) :
    System(
        "Pick",
        COMPONENTS::MaskOf<
            TransformComponent,
            PHYSICS::BoxColliderComponent,
            PHYSICS::SphereColliderComponent,
            PHYSICS::CylinderColliderComponent,
            PHYSICS::CapsuleColliderComponent
        >(),
        COMPONENTS::MaskOf<
            PHYSICS::PickedComponent,
            PHYSICS::PickRayComponent,
            PHYSICS::FallingTag>()
    ),
    boxColliderEntities(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::BoxColliderComponent>()),
    sphereColliderEntities(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::SphereColliderComponent>()),
    cylinderColliderEntities(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::CylinderColliderComponent>()),
    capsuleColliderEntities(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::CapsuleColliderComponent>()),
    pickedEntities(world, COMPONENTS::MaskOf<PHYSICS::PickedComponent>()),
    pickRays(world, COMPONENTS::MaskOf<PHYSICS::PickRayComponent>())
{
}

void PickSystem::Run(const SystemContext& context)
{
    const PHYSICS::Ray mouseRay = PHYSICS::MakePickRay(
        context.Get<InputState>().mousePosition,
        context.Get<InputState>().windowSize,
        context.Get<Camera>());

    const InputState& input = context.Get<InputState>();
    const bool drop = !input.mouseHeld[SDL_BUTTON_LEFT]
    || input.mousePressed[SDL_BUTTON_LEFT]
    || input.mousePressed[SDL_BUTTON_RIGHT];
    
    if (drop)
    {
        /* Remove picked from every holder since we are no longer holding the mouse */
        pickedEntities.ForEachChunk([&](const ChunkView chunk)
        {
            const Entity* owners = chunk.Entities();
            const uint32_t rows = chunk.Count();
            for (uint32_t row = 0; row < rows; ++row)
            {
                context.commands.RemoveComponent<PHYSICS::PickedComponent>(owners[row]);
            }
        });
    }
    else
    {
        /* Update the PickedComponent entity to have an updated position based on the mouse */
        pickedEntities.ForEachChunk([&](ChunkView chunk)
        {
            PHYSICS::PickedComponent* pickedComponents = chunk.Column<PHYSICS::PickedComponent>();
            const uint32_t rows = chunk.Count();
            for (uint32_t row = 0; row < rows; ++row)
            {
                pickedComponents[row].mouseTarget = mouseRay.PointAt(pickedComponents[row].grabT);
            }
        });
    }
    
    if (!context.Get<InputState>().mousePressed[SDL_BUTTON_LEFT]) return;
    
    float t = std::numeric_limits<float>::infinity();
    Entity pickedEntity{};
    PHYSICS::RayHit finalRayHit{};

    boxColliderEntities.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::BoxColliderComponent* colliders = chunk.Column<PHYSICS::BoxColliderComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            if (MinComponent(transforms[row].scale) < MATH::kVerySmall) continue;
            PHYSICS::Ray localRay = PHYSICS::ToLocalRay(mouseRay, transforms[row]);
            auto rayHit = PHYSICS::Intersect(localRay, colliders[row]);
            if (rayHit.has_value() && rayHit->t < t)
            {
                t = rayHit->t;
                finalRayHit = rayHit.value();
                pickedEntity = chunk.Entities()[row];
            }
        }
    });
    sphereColliderEntities.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::SphereColliderComponent* colliders = chunk.Column<PHYSICS::SphereColliderComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            if (MinComponent(transforms[row].scale) < MATH::kVerySmall) continue;
            PHYSICS::Ray localRay = PHYSICS::ToLocalRay(mouseRay, transforms[row]);
            auto rayHit = PHYSICS::Intersect(localRay, colliders[row]);
            if (rayHit.has_value() && rayHit->t < t)
            {
                t = rayHit->t;
                finalRayHit = rayHit.value();
                pickedEntity = chunk.Entities()[row];
            }
        }
    });
    cylinderColliderEntities.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::CylinderColliderComponent* colliders = chunk.Column<PHYSICS::CylinderColliderComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            if (MinComponent(transforms[row].scale) < MATH::kVerySmall) continue;
            PHYSICS::Ray localRay = PHYSICS::ToLocalRay(mouseRay, transforms[row]);
            auto rayHit = PHYSICS::Intersect(localRay, colliders[row]);
            if (rayHit.has_value() && rayHit->t < t)
            {
                t = rayHit->t;
                finalRayHit = rayHit.value();
                pickedEntity = chunk.Entities()[row];
            }
        }
    });
    capsuleColliderEntities.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::CapsuleColliderComponent* colliders = chunk.Column<PHYSICS::CapsuleColliderComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            if (MinComponent(transforms[row].scale) < MATH::kVerySmall) continue;
            PHYSICS::Ray localRay = PHYSICS::ToLocalRay(mouseRay, transforms[row]);
            auto rayHit = PHYSICS::Intersect(localRay, colliders[row]);
            if (rayHit.has_value() && rayHit->t < t)
            {
                t = rayHit->t;
                finalRayHit = rayHit.value();
                pickedEntity = chunk.Entities()[row];
            }
        }
    });
    
    const bool hasHit = pickedEntity.version != 0;
    if (hasHit)
    {
        auto colliderName = "CapsuleCollider";
        if (context.world.HasComponent<PHYSICS::BoxColliderComponent>(pickedEntity)) colliderName = "BoxCollider";
        else if (context.world.HasComponent<PHYSICS::SphereColliderComponent>(pickedEntity)) colliderName = "SphereCollider";
        else if (context.world.HasComponent<PHYSICS::CylinderColliderComponent>(pickedEntity)) colliderName = "CylinderCollider";

        context.commands.AddComponent<PHYSICS::PickedComponent>(
            pickedEntity,
            {
                .localPoint = finalRayHit.intersectionPosition,
                .grabT = finalRayHit.t,
                .mouseTarget = mouseRay.PointAt(finalRayHit.t)
            });
        context.commands.AddComponent<PHYSICS::FallingTag>(pickedEntity);
        
        /* Expected Format Picked entity 4 (BoxCollider): t = 7.31, world point (1.20, 0.50, -0.38) */
        const MATH::Vec3 worldPoint = mouseRay.PointAt(finalRayHit.t);
        std::cout << "Picked entity " << pickedEntity.index << " (" << colliderName << "): t = " << finalRayHit.t
            << ", world point (" << worldPoint.x << ", " << worldPoint.y << ", " << worldPoint.z << ")" << std::endl;
    }
    else
    {
        std::cout << "Nothing picked" << std::endl;
    }

    /* A miss draws the ray out to the far plane */
    pickRays.ForEachChunk([&](ChunkView chunk)
    {
        PHYSICS::PickRayComponent* pickRayComponents = chunk.Column<PHYSICS::PickRayComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            pickRayComponents[row] = {
                .ray = mouseRay,
                .t = hasHit ? finalRayHit.t : context.Get<Camera>().farPlane,
                .hasHit = hasHit
            };
        }
    });
}
