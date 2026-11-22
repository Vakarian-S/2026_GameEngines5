#include "game/systems/PlayerMoveSystem.h"

#include "game/Components.h"
#include "physics/Components.h"

PlayerMoveSystem::PlayerMoveSystem(World& world, const float speed_) :
    System("PlayerMove", COMPONENTS::MaskOf<PlayerCommand>(), COMPONENTS::MaskOf<PHYSICS::Velocity>()),
    entities(world, COMPONENTS::MaskOf<PlayerCommand, PHYSICS::Velocity>()),
    speed(speed_)
{
}

void PlayerMoveSystem::Run(const SystemContext& context)
{
    entities.ForEachChunk([this](ChunkView chunk)
    {
        PHYSICS::Velocity* velocities = chunk.Column<PHYSICS::Velocity>();
        const PlayerCommand* playerCommands = chunk.Column<PlayerCommand>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            velocities[row].metresPerSecond = playerCommands[row].moveDirection * speed;
        }
    });
}
