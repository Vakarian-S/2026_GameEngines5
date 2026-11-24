#include "physics/systems/UpdateOrientationSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/DeltaTime.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"
#include "physics/RigidBody.h"

UpdateOrientationSystem::UpdateOrientationSystem(World& world) :
    System("UpdateOrientation",
           COMPONENTS::MaskOf<
               TransformComponent,
               PHYSICS::AngularVelocityComponent
           >()
           , COMPONENTS::MaskOf<
               TransformComponent>()),
    entities(world, COMPONENTS::MaskOf<TransformComponent,
                                       PHYSICS::AngularVelocityComponent>())
{
}

void UpdateOrientationSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.Get<DeltaTime>().seconds;
    entities.ForEachChunk([&](ChunkView chunk)
    {
        TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::AngularVelocityComponent* angularVelocities = chunk.Column<PHYSICS::AngularVelocityComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            transforms[row].orientation = PHYSICS::UpdateOrientation(transforms[row].orientation,
                                                                     angularVelocities[row].radiansPerSecond,
                                                                     deltaTime);
        }
    });
}
