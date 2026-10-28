#include "core/systems/DrawRenderablesSystem.h"

#include <cassert>

#include "core/Components.h"
#include "core/DrawList.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"

DrawRenderablesSystem::DrawRenderablesSystem(World& world) :
    System("DrawRenderables", COMPONENTS::MaskOf<Transform, Renderable>(), 0),
    drawables(world, COMPONENTS::MaskOf<Transform, Renderable>())
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
        const Transform* transforms = archetype->Column<Transform>();
        const Renderable* renderables = archetype->Column<Renderable>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            const MATH::Matrix4 modelMatrixTransform = ModelMatrix(transforms[row]);
            drawList.push_back({renderables[row].meshHandle, renderables[row].materialHandle, modelMatrixTransform});
        }
    }
}
