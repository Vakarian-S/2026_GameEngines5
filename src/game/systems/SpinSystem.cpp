#include "game/systems/SpinSystem.h"

#include "ConstantsConversions.h"
#include "QMath.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "game/Components.h"

SpinSystem::SpinSystem(World& world) :
    System("Spin", COMPONENTS::MaskOf<Spin>(), COMPONENTS::MaskOf<Transform>()),
    spinners(world, COMPONENTS::MaskOf<Transform, Spin>())
{
}

void SpinSystem::Run(const SystemContext& context)
{
    const float deltaTime = context.deltaTime;
    constexpr MATH::Vec3 up(0.0f, 1.0f, 0.0f);
    for (Archetype* archetype : spinners.Matches())
    {
        Transform* transforms = archetype->Column<Transform>();
        const Spin* spins = archetype->Column<Spin>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            /* angleAxisRotation takes degrees */
            const MATH::Quaternion step = MATH::QMath::angleAxisRotation(
                spins[row].radiansPerSecond * deltaTime * RADIANS_TO_DEGREES, up);
            /* Step on the left turns about the world's up axis. Normalized so the length never drifts */
            transforms[row].orientation = MATH::QMath::normalize(step * transforms[row].orientation);
        }
    }
}
