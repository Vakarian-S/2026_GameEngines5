#include "physics/systems/UpdateOrientationSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"
#include "physics/RigidBody.h"

UpdateOrientationSystem::UpdateOrientationSystem(World& world) :
    System("UpdateOrientation",
           COMPONENTS::MaskOf<
               Transform,
               PHYSICS::AngularVelocity
           >()
           , COMPONENTS::MaskOf<
               Transform>()),
    entities(world, COMPONENTS::MaskOf<Transform,
                                       PHYSICS::AngularVelocity>())
{
}

void UpdateOrientationSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.deltaTime;
    entities.ForEachChunk([&](ChunkView chunk)
    {
        Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::AngularVelocity* angularVelocities = chunk.Column<PHYSICS::AngularVelocity>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            transforms[row].orientation = PHYSICS::UpdateOrientation(transforms[row].orientation,
                                                                     angularVelocities[row].radiansPerSecond,
                                                                     deltaTime);
        }
    });
}
