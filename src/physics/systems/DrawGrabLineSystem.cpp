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

    for (const Archetype* archetype : entities.Matches())
    {
        const Transform* transforms = archetype->Column<Transform>();
        const PHYSICS::Picked* pickedComponents = archetype->Column<PHYSICS::Picked>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            AddLine(debugLines, ModelMatrix(transforms[row]) * pickedComponents[row].localPoint,
                    pickedComponents[row].mouseTarget, kGrabLineColour);
        }
    }
}
