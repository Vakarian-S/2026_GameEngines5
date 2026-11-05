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
     * @return yellow when its entities hold Picked, otherwise green
     */
    MATH::Vec3 WireColour(const Archetype* archetype)
    {
        return (archetype->Mask() & COMPONENTS::MaskOf<PHYSICS::Picked>()) != 0 ? kYellowColour : kGreenColour;
    }
}

DrawCollidersSystem::DrawCollidersSystem(World& world) :
    System("DrawColliders",
           COMPONENTS::MaskOf<
           Transform,
           PHYSICS::BoxCollider,
           PHYSICS::SphereCollider,
           PHYSICS::CylinderCollider,
           PHYSICS::CapsuleCollider,
           PHYSICS::Picked
           >()
           , 0),
    boxColliderDrawables(world, COMPONENTS::MaskOf<Transform, PHYSICS::BoxCollider>()),
    sphereColliderDrawables(world, COMPONENTS::MaskOf<Transform, PHYSICS::SphereCollider>()),
    cylinderColliderDrawables(world, COMPONENTS::MaskOf<Transform, PHYSICS::CylinderCollider>()),
    capsuleColliderDrawables(
        world, COMPONENTS::MaskOf<Transform, PHYSICS::CapsuleCollider>())
{
}

void DrawCollidersSystem::Run(const SystemContext& context)
{
    assert(context.debugLines != nullptr && "DrawColliders runs in Extract, which hands it a debug lines list");
    DebugLines& debugLines = *context.debugLines;
    
    boxColliderDrawables.ForEachChunk([&](const ChunkView chunk)
    {
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::BoxCollider* colliders = chunk.Column<PHYSICS::BoxCollider>();
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
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::SphereCollider* colliders = chunk.Column<PHYSICS::SphereCollider>();
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
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::CylinderCollider* colliders = chunk.Column<PHYSICS::CylinderCollider>();
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
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::CapsuleCollider* colliders = chunk.Column<PHYSICS::CapsuleCollider>();
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
