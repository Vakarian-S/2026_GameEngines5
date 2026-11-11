#include "game/systems/SlideRightSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"

SlideRightSystem::SlideRightSystem(World& world, const char* name_, const ComponentMask rowTag, const float leftEdge_,
                                   const float pathWidth_) :
    System(name_, COMPONENTS::MaskOf<PHYSICS::Velocity>(), COMPONENTS::MaskOf<Transform>()),
    movers(world, COMPONENTS::MaskOf<Transform, PHYSICS::Velocity>() | rowTag),
    leftEdge(leftEdge_),
    pathWidth(pathWidth_)
{
}

void SlideRightSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.deltaTime;
    const float rightEdge = leftEdge + pathWidth;
    movers.ForEachChunk([&](ChunkView chunk)
    {
        Transform* transforms = chunk.Column<Transform>();
        const PHYSICS::Velocity* velocities = chunk.Column<PHYSICS::Velocity>();
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