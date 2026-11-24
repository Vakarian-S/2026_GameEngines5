#include "physics/systems/UpdatePositionSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/DeltaTime.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"
#include "physics/RigidBody.h"

UpdatePositionSystem::UpdatePositionSystem(World& world) :
    System("UpdatePosition", COMPONENTS::MaskOf<PHYSICS::VelocityComponent>(), COMPONENTS::MaskOf<TransformComponent>()),
    movers(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::VelocityComponent>())
{
}

void UpdatePositionSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.Get<DeltaTime>().seconds;
    movers.ForEachChunk([&](ChunkView chunk)
    {
        TransformComponent* transform = chunk.Column<TransformComponent>();
        const PHYSICS::VelocityComponent* velocities = chunk.Column<PHYSICS::VelocityComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            transform[row].position = PHYSICS::UpdatePosition(transform[row].position, velocities[row].metresPerSecond,
                                                              deltaTime);
        }
    });
}
