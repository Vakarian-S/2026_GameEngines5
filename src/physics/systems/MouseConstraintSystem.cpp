#include "physics/systems/MouseConstraintSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/DeltaTime.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"
#include "physics/RigidBody.h"

MouseConstraintSystem::MouseConstraintSystem(World& world) :
    System("MouseConstraint",
           COMPONENTS::MaskOf<
               PHYSICS::PickedComponent,
               TransformComponent,
               PHYSICS::VelocityComponent,
               PHYSICS::AngularVelocityComponent,
               PHYSICS::MassPropertiesComponent
           >()
           , COMPONENTS::MaskOf<PHYSICS::VelocityComponent, PHYSICS::AngularVelocityComponent>()),
    entities(world, COMPONENTS::MaskOf<PHYSICS::PickedComponent,
                                       TransformComponent,
                                       PHYSICS::VelocityComponent,
                                       PHYSICS::AngularVelocityComponent,
                                       PHYSICS::MassPropertiesComponent>())
{
}

void MouseConstraintSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.Get<DeltaTime>().seconds;
    entities.ForEachChunk([&](ChunkView chunk)
    {
        const TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::PickedComponent* pickedComponents = chunk.Column<PHYSICS::PickedComponent>();
        PHYSICS::VelocityComponent* velocities = chunk.Column<PHYSICS::VelocityComponent>();
        PHYSICS::AngularVelocityComponent* angularVelocities = chunk.Column<PHYSICS::AngularVelocityComponent>();
        const PHYSICS::MassPropertiesComponent* massProperties = chunk.Column<PHYSICS::MassPropertiesComponent>();
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
