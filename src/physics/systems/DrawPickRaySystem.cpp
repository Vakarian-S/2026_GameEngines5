#include "physics/systems/DrawPickRaySystem.h"

#include <cassert>

#include "VMath.h"
#include "core/debug/DebugLines.h"
#include "ecs/Archetype.h"
#include "ecs/SystemContext.h"
#include "physics/Components.h"

namespace
{
    constexpr auto kRayColour = MATH::Vec3(1.0f, 0.0f, 1.0f);
    constexpr auto kCrossColour = MATH::Vec3(1.0f, 1.0f, 1.0f);
    /** How far each arm of the hit cross reaches from its centre, in metres */
    constexpr float kCrossHalfSize = 0.15f;
}

DrawPickRaySystem::DrawPickRaySystem(World& world) :
    System("DrawPickRay", COMPONENTS::MaskOf<PHYSICS::PickRay>(), 0),
    pickRays(world, COMPONENTS::MaskOf<PHYSICS::PickRay>())
{
}

void DrawPickRaySystem::Run(const SystemContext& context)
{
    assert(context.debugLines != nullptr && "DrawPickRay runs in Extract, which hands it a debug lines list");
    DebugLines& debugLines = *context.debugLines;

    for (const Archetype* archetype : pickRays.Matches())
    {
        const PHYSICS::PickRay* pickRayComponents = archetype->Column<PHYSICS::PickRay>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            const auto& [ray, t, hasHit] = pickRayComponents[row];
            /* A zero direction means nobody has clicked yet */
            if (MATH::VMath::dot(ray.direction, ray.direction) < MATH::kVerySmall) continue;

            const MATH::Vec3 end = ray.PointAt(t);
            AddLine(debugLines, ray.origin, end, kRayColour);
            if (!hasHit) continue;

            constexpr MATH::Vec3 armX(kCrossHalfSize, 0.0f, 0.0f);
            constexpr MATH::Vec3 armY(0.0f, kCrossHalfSize, 0.0f);
            constexpr MATH::Vec3 armZ(0.0f, 0.0f, kCrossHalfSize);
            AddLine(debugLines, end - armX, end + armX, kCrossColour);
            AddLine(debugLines, end - armY, end + armY, kCrossColour);
            AddLine(debugLines, end - armZ, end + armZ, kCrossColour);
        }
    }
}
