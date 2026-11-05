#include "physics/systems/UpdatePositionSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"
#include "physics/RigidBody.h"

UpdatePositionSystem::UpdatePositionSystem(World& world) :
    System("UpdatePosition", COMPONENTS::MaskOf<PHYSICS::Velocity>(), COMPONENTS::MaskOf<Transform>()),
    movers(world, COMPONENTS::MaskOf<Transform, PHYSICS::Velocity>())
{
}

void UpdatePositionSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.deltaTime;
    movers.ForEachChunk([&](ChunkView chunk)
    {
        Transform* transform = chunk.Column<Transform>();
        const PHYSICS::Velocity* velocities = chunk.Column<PHYSICS::Velocity>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            transform[row].position = PHYSICS::UpdatePosition(transform[row].position, velocities[row].metresPerSecond,
                                                              deltaTime);
        }
    });
}
