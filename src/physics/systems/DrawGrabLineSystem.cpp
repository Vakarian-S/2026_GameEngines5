#include "physics/systems/DrawGrabLineSystem.h"
#include <cassert>

#include "core/Components.h"
#include "core/debug/DebugLines.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"

namespace
{
    constexpr auto kGrabLineColour = MATH::Vec3(0.0f, 0.5f, 0.8f);
}

DrawGrabLineSystem::DrawGrabLineSystem(World& world) :
    System("DrawGrabLine",
           COMPONENTS::MaskOf<
               PHYSICS::PickedComponent,
               TransformComponent
           >()
           , 0),
    entities(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::PickedComponent>())
{
}

void DrawGrabLineSystem::Run(const SystemContext& context)
{
    assert(context.Get<DebugLinesTarget>().lines != nullptr && "DrawGrabLine runs in Extract, which hands it a debug lines list");
    DebugLines& debugLines = *context.Get<DebugLinesTarget>().lines;

    entities.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::PickedComponent* pickedComponents = chunk.Column<PHYSICS::PickedComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            AddLine(debugLines, ModelMatrix(transforms[row]) * pickedComponents[row].localPoint,
                    pickedComponents[row].mouseTarget, kGrabLineColour);
        }
    });
}
