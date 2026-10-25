#include "game/systems/MovementSystem.h"

#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"

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
    System("Movement", COMPONENTS::MaskOf<Velocity>(), COMPONENTS::MaskOf<Position>()),
    movers(world, COMPONENTS::MaskOf<Position, Velocity>()),
    boxHalfWidth(boxHalfWidth_)
{
}

void MovementSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.deltaTime;
    for (Archetype* archetype : movers.Matches())
    {
        Position* positions = archetype->Column<Position>();
        const Velocity* velocities = archetype->Column<Velocity>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            positions[row].x = WrapIntoBox(positions[row].x + velocities[row].x * deltaTime, boxHalfWidth);
            positions[row].y = WrapIntoBox(positions[row].y + velocities[row].y * deltaTime, boxHalfWidth);
            positions[row].z = WrapIntoBox(positions[row].z + velocities[row].z * deltaTime, boxHalfWidth);
        }
    }
}