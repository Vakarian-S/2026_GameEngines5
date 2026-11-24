#include "game/systems/SlideRightSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/DeltaTime.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"

SlideRightSystem::SlideRightSystem(World& world, const char* name_, const ComponentMask rowTag, const float leftEdge_,
                                   const float pathWidth_) :
    System(name_, COMPONENTS::MaskOf<PHYSICS::VelocityComponent>(), COMPONENTS::MaskOf<TransformComponent>()),
    movers(world, COMPONENTS::MaskOf<TransformComponent, PHYSICS::VelocityComponent>() | rowTag),
    leftEdge(leftEdge_),
    pathWidth(pathWidth_)
{
}

void SlideRightSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.Get<DeltaTime>().seconds;
    const float rightEdge = leftEdge + pathWidth;
    movers.ForEachChunk([&](ChunkView chunk)
    {
        TransformComponent* transforms = chunk.Column<TransformComponent>();
        const PHYSICS::VelocityComponent* velocities = chunk.Column<PHYSICS::VelocityComponent>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            float& x = transforms[row].position.x;
            x += velocities[row].metresPerSecond.x * deltaTime;
            /* Subtracting the width keeps the distance past the edge, so the spacing never drifts */
            if (x > rightEdge) x -= pathWidth;
        }
    });
}
