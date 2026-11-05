#include "physics/systems/MouseConstraintSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"
#include "physics/RigidBody.h"

MouseConstraintSystem::MouseConstraintSystem(World& world) :
    System("MouseConstraint",
           COMPONENTS::MaskOf<
               PHYSICS::Picked,
               Transform,
               PHYSICS::Velocity,
               PHYSICS::AngularVelocity,
               PHYSICS::MassProperties
           >()
           , COMPONENTS::MaskOf<PHYSICS::Velocity, PHYSICS::AngularVelocity>()),
    entities(world, COMPONENTS::MaskOf<PHYSICS::Picked,
                                       Transform,
                                       PHYSICS::Velocity,
                                       PHYSICS::AngularVelocity,
                                       PHYSICS::MassProperties>())
{
}

void MouseConstraintSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.deltaTime;
    entities.ForEachChunk([&](ChunkView chunk)
    {
        const Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::Picked* pickedComponents = chunk.Column<PHYSICS::Picked>();
        PHYSICS::Velocity* velocities = chunk.Column<PHYSICS::Velocity>();
        PHYSICS::AngularVelocity* angularVelocities = chunk.Column<PHYSICS::AngularVelocity>();
        const PHYSICS::MassProperties* massProperties = chunk.Column<PHYSICS::MassProperties>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            angularVelocities[row].radiansPerSecond = PHYSICS::ApplyGrip(angularVelocities[row].radiansPerSecond, deltaTime);
            const auto [linear, angular] = PHYSICS::MouseConstraint(
                transforms[row],
                pickedComponents[row].localPoint,
                pickedComponents[row].mouseTarget,
                massProperties[row],
                velocities[row].metresPerSecond,
                angularVelocities[row].radiansPerSecond,
                deltaTime);
            velocities[row].metresPerSecond = linear;
            angularVelocities[row].radiansPerSecond = PHYSICS::LimitSpin(angular);
        }
    });
}
