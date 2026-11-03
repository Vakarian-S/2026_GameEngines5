#include "physics/systems/ApplyForcesSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"
#include "physics/RigidBody.h"

ApplyForcesSystem::ApplyForcesSystem(World& world) :
    System("ApplyForces",
           COMPONENTS::MaskOf<
               PHYSICS::Velocity,
               PHYSICS::AngularVelocity,
               PHYSICS::MassProperties,
               PHYSICS::Falling
           >()
           , COMPONENTS::MaskOf<PHYSICS::Velocity, PHYSICS::AngularVelocity>()),
    entities(
        world,
        COMPONENTS::MaskOf<PHYSICS::Velocity, PHYSICS::AngularVelocity, PHYSICS::MassProperties, PHYSICS::Falling>())
{
}

void ApplyForcesSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.deltaTime;
    for (Archetype* archetype : entities.Matches())
    {
        PHYSICS::Velocity* velocities = archetype->Column<PHYSICS::Velocity>();
        PHYSICS::AngularVelocity* angularVelocities = archetype->Column<PHYSICS::AngularVelocity>();
        const PHYSICS::MassProperties* massProperties = archetype->Column<PHYSICS::MassProperties>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            velocities[row].metresPerSecond = PHYSICS::ApplyForces(velocities[row].metresPerSecond,
                                                                   massProperties[row].mass,
                                                                   deltaTime);
            angularVelocities[row].radiansPerSecond = PHYSICS::ApplyAngularDrag(
                angularVelocities[row].radiansPerSecond, deltaTime);
        }
    }
}
