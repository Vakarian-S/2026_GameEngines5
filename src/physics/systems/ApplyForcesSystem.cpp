#include "physics/systems/ApplyForcesSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/DeltaTime.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"
#include "physics/RigidBody.h"

ApplyForcesSystem::ApplyForcesSystem(World& world) :
    System("ApplyForces",
           COMPONENTS::MaskOf<
               PHYSICS::VelocityComponent,
               PHYSICS::AngularVelocityComponent,
               PHYSICS::MassPropertiesComponent,
               PHYSICS::FallingTag
           >()
           , COMPONENTS::MaskOf<PHYSICS::VelocityComponent, PHYSICS::AngularVelocityComponent>()),
    entities(
        world,
        COMPONENTS::MaskOf<PHYSICS::VelocityComponent, PHYSICS::AngularVelocityComponent, PHYSICS::MassPropertiesComponent, PHYSICS::FallingTag>())
{
}

void ApplyForcesSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.Get<DeltaTime>().seconds;
    entities.ForEachChunk([&](ChunkView chunk)
    {
        PHYSICS::VelocityComponent* velocities = chunk.Column<PHYSICS::VelocityComponent>();
        PHYSICS::AngularVelocityComponent* angularVelocities = chunk.Column<PHYSICS::AngularVelocityComponent>();
        const PHYSICS::MassPropertiesComponent* massProperties = chunk.Column<PHYSICS::MassPropertiesComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            velocities[row].metresPerSecond = PHYSICS::ApplyForces(velocities[row].metresPerSecond,
                                                                   massProperties[row].mass,
                                                                   deltaTime);
            angularVelocities[row].radiansPerSecond = PHYSICS::ApplyAngularDrag(
                angularVelocities[row].radiansPerSecond, deltaTime);
        }
    });
}
