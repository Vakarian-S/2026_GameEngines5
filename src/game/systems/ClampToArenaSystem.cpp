#include "game/systems/ClampToArenaSystem.h"

#include "core/Components.h"
#include "game/Components.h"

#include <algorithm>

ClampToArenaSystem::ClampToArenaSystem(World& world, const float halfWidth_, const float halfDepth_) :
    System("ClampToArena", COMPONENTS::MaskOf<TransformComponent>(), COMPONENTS::MaskOf<TransformComponent>()),
    entities(world, COMPONENTS::MaskOf<TransformComponent, PlayerCommandComponent>()),
    halfWidth(halfWidth_),
    halfDepth(halfDepth_)

{
}

void ClampToArenaSystem::Run(const SystemContext& context)
{
    entities.ForEachChunk([this](ChunkView chunk)
    {
        TransformComponent* transforms = chunk.Column<TransformComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            TransformComponent& transform = transforms[row];
            transform.position.x = std::clamp(transform.position.x, -halfWidth, halfWidth);
            transform.position.z = std::clamp(transform.position.z, -halfDepth, halfDepth);
        }
    });
}
