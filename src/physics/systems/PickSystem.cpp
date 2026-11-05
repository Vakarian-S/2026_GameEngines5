#include "physics/systems/PickSystem.h"

#include <SDL3/SDL_mouse.h>
#include <limits>
#include <iostream>

#include "core/Camera.h"
#include "core/Components.h"
#include "core/InputState.h"
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
            Transform,
            PHYSICS::BoxCollider,
            PHYSICS::SphereCollider,
            PHYSICS::CylinderCollider,
            PHYSICS::CapsuleCollider
        >(),
        COMPONENTS::MaskOf<
            PHYSICS::Picked,
            PHYSICS::PickRay,
            PHYSICS::Falling>()
    ),
    boxColliderEntities(world, COMPONENTS::MaskOf<Transform, PHYSICS::BoxCollider>()),
    sphereColliderEntities(world, COMPONENTS::MaskOf<Transform, PHYSICS::SphereCollider>()),
    cylinderColliderEntities(world, COMPONENTS::MaskOf<Transform, PHYSICS::CylinderCollider>()),
    capsuleColliderEntities(world, COMPONENTS::MaskOf<Transform, PHYSICS::CapsuleCollider>()),
    pickedEntities(world, COMPONENTS::MaskOf<PHYSICS::Picked>()),
    pickRays(world, COMPONENTS::MaskOf<PHYSICS::PickRay>())
{
}

void PickSystem::Run(const SystemContext& context)
{
    const PHYSICS::Ray mouseRay = PHYSICS::MakePickRay(
        context.inputState->mousePosition,
        context.inputState->windowSize,
        *context.camera);

    auto input = context.inputState;
    const bool drop = !input->mouseHeld[SDL_BUTTON_LEFT]
    || input->mousePressed[SDL_BUTTON_LEFT]
    || input->mousePressed[SDL_BUTTON_RIGHT];
    
    if (drop)
    {
        /* Remove picked from every holder since we are no longer holding the mouse */
        pickedEntities.ForEachChunk([&](const ChunkView chunk)
        {
            const Entity* owners = chunk.Entities();
            const uint32_t rows = chunk.Count();
            for (uint32_t row = 0; row < rows; ++row)
            {
                context.commands.RemoveComponent<PHYSICS::Picked>(owners[row]);
            }
        });
    }
    else
    {
        /* Update the Picked entity to have an updated position based on the mouse */
        pickedEntities.ForEachChunk([&](ChunkView chunk)
        {
            PHYSICS::Picked* pickedComponents = chunk.Column<PHYSICS::Picked>();
            const uint32_t rows = chunk.Count();
            for (uint32_t row = 0; row < rows; ++row)
            {
                pickedComponents[row].mouseTarget = mouseRay.PointAt(pickedComponents[row].grabT);
            }
        });
    }
    
    if (!context.inputState->mousePressed[SDL_BUTTON_LEFT]) return;
    
    float t = std::numeric_limits<float>::infinity();
    Entity pickedEntity{};
    PHYSICS::RayHit finalRayHit{};

    boxColliderEntities.ForEachChunk([&](const ChunkView chunk)
    {
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::BoxCollider* colliders = chunk.Column<PHYSICS::BoxCollider>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            if (transforms[row].scale < MATH::kVerySmall) continue;
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
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::SphereCollider* colliders = chunk.Column<PHYSICS::SphereCollider>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            if (transforms[row].scale < MATH::kVerySmall) continue;
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
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::CylinderCollider* colliders = chunk.Column<PHYSICS::CylinderCollider>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            if (transforms[row].scale < MATH::kVerySmall) continue;
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
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::CapsuleCollider* colliders = chunk.Column<PHYSICS::CapsuleCollider>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            if (transforms[row].scale < MATH::kVerySmall) continue;
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
        if (context.world.HasComponent<PHYSICS::BoxCollider>(pickedEntity)) colliderName = "BoxCollider";
        else if (context.world.HasComponent<PHYSICS::SphereCollider>(pickedEntity)) colliderName = "SphereCollider";
        else if (context.world.HasComponent<PHYSICS::CylinderCollider>(pickedEntity)) colliderName = "CylinderCollider";

        context.commands.AddComponent<PHYSICS::Picked>(
            pickedEntity,
            {
                .localPoint = finalRayHit.intersectionPosition,
                .grabT = finalRayHit.t,
                .mouseTarget = mouseRay.PointAt(finalRayHit.t)
            });
        context.commands.AddComponent<PHYSICS::Falling>(pickedEntity);
        
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
        PHYSICS::PickRay* pickRayComponents = chunk.Column<PHYSICS::PickRay>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            pickRayComponents[row] = {
                .ray = mouseRay,
                .t = hasHit ? finalRayHit.t : context.camera->farPlane,
                .hasHit = hasHit
            };
        }
    });
}
