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
    for (Archetype* archetype : entities.Matches())
    {
        const Transform* transforms = archetype->Column<Transform>();
        const PHYSICS::Picked* pickedComponents = archetype->Column<PHYSICS::Picked>();
        PHYSICS::Velocity* velocities = archetype->Column<PHYSICS::Velocity>();
        PHYSICS::AngularVelocity* angularVelocities = archetype->Column<PHYSICS::AngularVelocity>();
        const PHYSICS::MassProperties* massProperties = archetype->Column<PHYSICS::MassProperties>();
        const uint32_t rows = archetype->Count();
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
    }
}
