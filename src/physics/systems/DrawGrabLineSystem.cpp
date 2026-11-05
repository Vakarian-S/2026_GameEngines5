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
               PHYSICS::Picked,
               Transform
           >()
           , 0),
    entities(world, COMPONENTS::MaskOf<Transform, PHYSICS::Picked>())
{
}

void DrawGrabLineSystem::Run(const SystemContext& context)
{
    assert(context.debugLines != nullptr && "DrawGrabLine runs in Extract, which hands it a debug lines list");
    DebugLines& debugLines = *context.debugLines;

    entities.ForEachChunk([&](const ChunkView chunk)
    {
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::Picked* pickedComponents = chunk.Column<PHYSICS::Picked>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            AddLine(debugLines, ModelMatrix(transforms[row]) * pickedComponents[row].localPoint,
                    pickedComponents[row].mouseTarget, kGrabLineColour);
        }
    });
}
