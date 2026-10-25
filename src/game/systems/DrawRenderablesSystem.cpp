#include "game/systems/DrawRenderablesSystem.h"

#include <cassert>

#include "ConstantsConversions.h"
#include "MMath.h"
#include "core/DrawList.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"

DrawRenderablesSystem::DrawRenderablesSystem(World& world) :
    System("DrawRenderables", COMPONENTS::MaskOf<Position, Rotation, Renderable>(), 0),
    drawables(world, COMPONENTS::MaskOf<Position, Rotation, Renderable>())
{
}

void DrawRenderablesSystem::Run(const SystemContext& context)
{
    assert(context.drawList != nullptr && "DrawRenderables runs in Extract, which hands it a draw list");
    DrawList& drawList = *context.drawList;

    uint32_t total = 0;
    for (const Archetype* archetype : drawables.Matches()) total += archetype->Count();

    /* Reserved once, so the list takes one block from the frame arena */
    drawList.reserve(drawList.size() + total);

    for (const Archetype* archetype : drawables.Matches())
    {
        const Position* positions = archetype->Column<Position>();
        const Rotation* rotations = archetype->Column<Rotation>();
        const Renderable* renderables = archetype->Column<Renderable>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            /* Turns the mesh about its own centre first, then moves it into place. MMath::rotate takes degrees */
            const MATH::Matrix4 transform =
                MATH::MMath::translate(positions[row].x, positions[row].y, positions[row].z) *
                MATH::MMath::rotate(rotations[row].yawRadians * RADIANS_TO_DEGREES, 0.0f, 1.0f, 0.0f);
            drawList.push_back({renderables[row].meshHandle, renderables[row].materialHandle, transform});
        }
    }
}