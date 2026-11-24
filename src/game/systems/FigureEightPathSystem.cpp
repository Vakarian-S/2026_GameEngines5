#include "game/systems/FigureEightPathSystem.h"

#include "core/Components.h"
#include "core/time/FixedTickCount.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"
#include "game/FigureEightPath.h"

FigureEightPathSystem::FigureEightPathSystem(World& world) :
    System("FigureEightPath", COMPONENTS::MaskOf<FigureEightPathComponent>(), COMPONENTS::MaskOf<TransformComponent>()),
    entities(world, COMPONENTS::MaskOf<TransformComponent, FigureEightPathComponent>())
{
}

void FigureEightPathSystem::Run(const SystemContext& context)
{
    const std::uint64_t tick = context.Get<FixedTickCount>().ticks;
    entities.ForEachChunk([tick](ChunkView chunk)
    {
        TransformComponent* transforms = chunk.Column<TransformComponent>();
        const FigureEightPathComponent* paths = chunk.Column<FigureEightPathComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            const FigureEightPathComponent& path = paths[row];
            transforms[row].position = path.center +
                FigureEightPathPosition(tick, path.periodTicks, path.halfWidth, path.halfDepth);
        }
    });
}
