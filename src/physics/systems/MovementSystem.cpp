#include "physics/systems/MovementSystem.h"

#include "core/Components.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"

namespace
{
    /**
     * Brings a coordinate that left the box back in through the opposite wall.
     * @param value one coordinate of a position
     * @param halfWidth half the width of the box
     * @return the coordinate, inside the box
     */
    float WrapIntoBox(const float value, const float halfWidth)
    {
        if (value > halfWidth) return value - 2.0f * halfWidth;
        if (value < -halfWidth) return value + 2.0f * halfWidth;
        return value;
    }
}

MovementSystem::MovementSystem(World& world, const float boxHalfWidth_) :
    System("Movement", COMPONENTS::MaskOf<PHYSICS::Velocity>(), COMPONENTS::MaskOf<Transform>()),
    movers(world, COMPONENTS::MaskOf<Transform, PHYSICS::Velocity>()),
    boxHalfWidth(boxHalfWidth_)
{
}

void MovementSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.deltaTime;
    const float halfWidth = boxHalfWidth;
    for (Archetype* archetype : movers.Matches())
    {
        Transform* transform = archetype->Column<Transform>();
        const PHYSICS::Velocity* velocities = archetype->Column<PHYSICS::Velocity>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            transform[row].position.x = WrapIntoBox(transform[row].position.x + velocities[row].metresPerSecond.x * deltaTime, halfWidth);
            transform[row].position.y = WrapIntoBox(transform[row].position.y + velocities[row].metresPerSecond.y * deltaTime, halfWidth);
            transform[row].position.z = WrapIntoBox(transform[row].position.z + velocities[row].metresPerSecond.z * deltaTime, halfWidth);
        }
    }
}