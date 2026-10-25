#include "game/systems/SpinSystem.h"

#include "ConstantsConversions.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"

SpinSystem::SpinSystem(World& world) :
    System("Spin", COMPONENTS::MaskOf<Spin>(), COMPONENTS::MaskOf<Rotation>()),
    spinners(world, COMPONENTS::MaskOf<Rotation, Spin>())
{
}

void SpinSystem::Run(const SystemContext& context)
{
    for (Archetype* archetype : spinners.Matches())
    {
        Rotation* rotations = archetype->Column<Rotation>();
        const Spin* spins = archetype->Column<Spin>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            rotations[row].yawRadians += spins[row].radiansPerSecond * context.deltaTime;
            /* Kept inside one turn, so the float does not lose precision as the angle grows */
            if (rotations[row].yawRadians > 2.0f * FLOAT_PI) rotations[row].yawRadians -= 2.0f * FLOAT_PI;
        }
    }
}