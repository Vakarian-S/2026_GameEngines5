#include "physics/systems/DrawCollidersSystem.h"
#include <cassert>
#include "core/Components.h"
#include "core/debug/DebugLines.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"

namespace
{
    constexpr auto kGreenColour = MATH::Vec3(0.0f, 1.0f, 0.0f);
    constexpr auto kYellowColour = MATH::Vec3(1.0f, 1.0f, 0.0f);

    /**
     * Picks the wire colour for a whole archetype. Every row in an archetype has the same components
     * @param archetype archetype about to be drawn
     * @return yellow when its entities hold PickedComponent, otherwise green
     */
    MATH::Vec3 WireColour(const Archetype* archetype)
    {
        return (archetype->Mask() & COMPONENTS::MaskOf<PHYSICS::PickedComponent>()) != 0 ? kYellowColour : kGreenColour;
    }
}

DrawCollidersSystem::DrawCollidersSystem(World& world) :
    System("DrawColliders",
           COMPONENTS::MaskOf<
           TransformComponent,
           PHYSICS::BoxColliderComponent,
           PHYSICS::SphereColliderComponent,
           PHYSICS::CylinderColliderComponent,
           PHYSICS::CapsuleColliderComponent,
           PHYSICS::PickedComponent
           >()
           , 0),
    boxColliderDrawables(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::BoxColliderComponent>()),
    sphereColliderDrawables(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::SphereColliderComponent>()),
    cylinderColliderDrawables(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::CylinderColliderComponent>()),
    capsuleColliderDrawables(
        world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::CapsuleColliderComponent>())
{
}

void DrawCollidersSystem::Run(const SystemContext& context)
{
    assert(context.Get<DebugLinesTarget>().lines != nullptr && "DrawColliders runs in Extract, which hands it a debug lines list");
    DebugLines& debugLines = *context.Get<DebugLinesTarget>().lines;
    
    boxColliderDrawables.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::BoxColliderComponent* colliders = chunk.Column<PHYSICS::BoxColliderComponent>();
        const MATH::Vec3 colour = WireColour(chunk.Owner());
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            const MATH::Matrix4 modelMatrixTransform = ModelMatrix(transforms[row]);
            AddWireBox(debugLines, modelMatrixTransform, colliders[row].halfExtents, colour);
        }
    });

    sphereColliderDrawables.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::SphereColliderComponent* colliders = chunk.Column<PHYSICS::SphereColliderComponent>();
        const MATH::Vec3 colour = WireColour(chunk.Owner());
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            const MATH::Matrix4 modelMatrixTransform = ModelMatrix(transforms[row]);
            AddWireSphere(debugLines, modelMatrixTransform, colliders[row].radius, colour);
        }
    });

    cylinderColliderDrawables.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::CylinderColliderComponent* colliders = chunk.Column<PHYSICS::CylinderColliderComponent>();
        const MATH::Vec3 colour = WireColour(chunk.Owner());
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            const MATH::Matrix4 modelMatrixTransform = ModelMatrix(transforms[row]);
            AddWireCylinder(debugLines, modelMatrixTransform, colliders[row].radius, colliders[row].halfHeight,
                            colour);
        }
    });

    capsuleColliderDrawables.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::CapsuleColliderComponent* colliders = chunk.Column<PHYSICS::CapsuleColliderComponent>();
        const MATH::Vec3 colour = WireColour(chunk.Owner());
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            const MATH::Matrix4 modelMatrixTransform = ModelMatrix(transforms[row]);
            AddWireCapsule(debugLines, modelMatrixTransform, colliders[row].radius, colliders[row].halfHeight,
                           colour);
        }
    });
}
