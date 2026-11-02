#include "game/systems/WrapIntoBoxSystem.h"

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

WrapIntoBoxSystem::WrapIntoBoxSystem(World& world, const float boxHalfWidth_) :
    System("WrapIntoBox", 0, COMPONENTS::MaskOf<Transform>()),
    movers(world, COMPONENTS::MaskOf<Transform, PHYSICS::Velocity>()),
    boxHalfWidth(boxHalfWidth_)
{
}

void WrapIntoBoxSystem::Run(const SystemContext& context)
{
    const float halfWidth = boxHalfWidth;
    for (Archetype* archetype : movers.Matches())
    {
        Transform* transforms = archetype->Column<Transform>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            MATH::Vec3& position = transforms[row].position;
            position.x = WrapIntoBox(position.x, halfWidth);
            position.y = WrapIntoBox(position.y, halfWidth);
            position.z = WrapIntoBox(position.z, halfWidth);
        }
    }
}
