#include "core/systems/DrawRenderablesSystem.h"

#include <cassert>

#include "core/Components.h"
#include "core/DrawList.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"

DrawRenderablesSystem::DrawRenderablesSystem(World& world) :
    System("DrawRenderables", COMPONENTS::MaskOf<TransformComponent, RenderableComponent>(), 0),
    drawables(world, COMPONENTS::MaskOf<TransformComponent, RenderableComponent>())
{
}

void DrawRenderablesSystem::Run(const SystemContext& context)
{
    assert(
        context.Get<DrawListTarget>().list != nullptr && "DrawRenderables runs in Extract, which hands it a draw list");
    DrawList& drawList = *context.Get<DrawListTarget>().list;

    uint32_t total = 0;
    for (const Archetype* archetype : drawables.Matches()) total += archetype->Count();

    /* Reserved once, so the list takes one block from the frame arena */
    drawList.reserve(drawList.size() + total);

    drawables.ForEachChunk([&](const ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const RenderableComponent* renderables = chunk.Column<RenderableComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            const MATH::Matrix4 modelMatrixTransform = ModelMatrix(transforms[row]);
            drawList.push_back({
                renderables[row].meshHandle, renderables[row].materialHandle, modelMatrixTransform,
                renderables[row].castsShadow
            });
        }
    });
}
