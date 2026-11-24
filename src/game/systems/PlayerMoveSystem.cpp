#include "game/systems/PlayerMoveSystem.h"

#include "game/Components.h"
#include "physics/Components.h"

PlayerMoveSystem::PlayerMoveSystem(World& world, const float speed_) :
    System("PlayerMove", COMPONENTS::MaskOf<PlayerCommandComponent>(), COMPONENTS::MaskOf<PHYSICS::VelocityComponent>()),
    entities(world, COMPONENTS::MaskOf<PlayerCommandComponent, PHYSICS::VelocityComponent>()),
    speed(speed_)
{
}

void PlayerMoveSystem::Run(const SystemContext& context)
{
    entities.ForEachChunk([this](ChunkView chunk)
    {
        PHYSICS::VelocityComponent* velocities = chunk.Column<PHYSICS::VelocityComponent>();
        const PlayerCommandComponent* playerCommands = chunk.Column<PlayerCommandComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            velocities[row].metresPerSecond = playerCommands[row].moveDirection * speed;
        }
    });
}
