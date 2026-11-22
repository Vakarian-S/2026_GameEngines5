#include "game/systems/ClampToArenaSystem.h"

#include "core/Components.h"
#include "game/Components.h"

#include <algorithm>

ClampToArenaSystem::ClampToArenaSystem(World& world, const float halfWidth_, const float halfDepth_) :
    System("ClampToArena", COMPONENTS::MaskOf<Transform>(), COMPONENTS::MaskOf<Transform>()),
    entities(world, COMPONENTS::MaskOf<Transform, PlayerCommand>()),
    halfWidth(halfWidth_),
    halfDepth(halfDepth_)

{
}

void ClampToArenaSystem::Run(const SystemContext& context)
{
    entities.ForEachChunk([this](ChunkView chunk)
    {
        Transform* transforms = chunk.Column<Transform>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            Transform& transform = transforms[row];
            transform.position.x = std::clamp(transform.position.x, -halfWidth, halfWidth);
            transform.position.z = std::clamp(transform.position.z, -halfDepth, halfDepth);
        }
    });
}
