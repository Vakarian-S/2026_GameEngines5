#include "core/debug/DebugLines.h"
#include "ConstantsConversions.h"
#include <cmath>

namespace
{
    /** Number of straight pieces in one full wire circle */
    constexpr int kCircleSegments = 32;
    


    /**
     * Adds a segmented arc to the debug rendering lines.
     *
     * @param lines The container for debug line vertices where the arc will be added.
     * @param world The transformation matrix applied to the arc, transforming it into world space.
     * @param center The centre of the arc in 3D space.
     * @param radius The radius of the arc.
     * @param xAxis The vector representing the local x-axis of the arc orientation.
     * @param yAxis The vector representing the local y-axis of the arc orientation.
     * @param sweep The angular sweep of the arc in radians.
     * @param segments The number of line segments used to approximate the arc.
     * @param color The colour of the arc as a 3D vector.
     */
    void AddArc(DebugLines& lines, const MATH::Matrix4& world, const MATH::Vec3& center, const float radius,
                const MATH::Vec3& xAxis, const MATH::Vec3& yAxis, const float sweep, const int segments,
                const MATH::Vec3& color)
    {
        MATH::Vec3 prev = world * (center + radius * xAxis);
        for (int i = 1; i <= segments; ++i)
        {
            const float angle = sweep * static_cast<float>(i) / static_cast<float>(segments);
            MATH::Vec3 current = world * (center + radius * (std::cos(angle) * xAxis +
                std::sin(angle) * yAxis));
            AddLine(lines, prev, current, color);
            prev = current;
        }
    }
}

void AddLine(DebugLines& lines, const MATH::Vec3& from, const MATH::Vec3& to, const MATH::Vec3& color)
{
    lines.push_back({.position = from, .color = color});
    lines.push_back({.position = to, .color = color});
}

void AddAxes(DebugLines& lines, const float length)
{
    AddLine(lines, MATH::Vec3(0.f, 0.f, 0.f), MATH::Vec3(length, 0.f, 0.f), MATH::Vec3(1.f, 0.f, 0.f));
    AddLine(lines, MATH::Vec3(0.f, 0.f, 0.f), MATH::Vec3(0.f, length, 0.f), MATH::Vec3(0.f, 1.f, 0.f));
    AddLine(lines, MATH::Vec3(0.f, 0.f, 0.f), MATH::Vec3(0.f, 0.f, length), MATH::Vec3(0.f, 0.f, 1.f));
}

void AddGrid(DebugLines& lines, const float halfSize, const float spacing, const MATH::Vec3& color)
{
    const int n = static_cast<int>(halfSize / spacing);
    for (int i = -n; i <= n; ++i)
    {
        AddLine(lines, MATH::Vec3(-halfSize, 0.f, static_cast<float>(i) * spacing),
                MATH::Vec3(halfSize, 0.f, static_cast<float>(i) * spacing), color);
        AddLine(lines, MATH::Vec3(static_cast<float>(i) * spacing, 0.f, -halfSize),
                MATH::Vec3(static_cast<float>(i) * spacing, 0.f, halfSize), color);
    }
}

void AddWireBox(DebugLines& lines, const MATH::Matrix4& world, const MATH::Vec3 halfExtents, const MATH::Vec3& color)
{
    /* Bit 0 of a corner's index picks the sign of x, bit 1 the sign of y, bit 2 the sign of z */
    MATH::Vec3 corners[8];
    for (int i = 0; i < 8; ++i)
    {
        const MATH::Vec3 local(i & 1 ? halfExtents.x : -halfExtents.x,
                               i & 2 ? halfExtents.y : -halfExtents.y,
                               i & 4 ? halfExtents.z : -halfExtents.z);
        corners[i] = world * local;
    }
    /* Two corners share an edge when their indices differ in exactly one bit */
    for (int axis = 0; axis < 3; ++axis)
    {
        const int bit = 1 << axis;
        for (int i = 0; i < 8; ++i)
        {
            if ((i & bit) == 0) AddLine(lines, corners[i], corners[i | bit], color);
        }
    }
}

void AddWireSphere(DebugLines& lines, const MATH::Matrix4& world, const float radius, const MATH::Vec3& color)
{
    /* XY */
    AddArc(lines, world, MATH::Vec3(), radius, MATH::Vec3(1.f, 0.f, 0.f), MATH::Vec3(0.f, 1.f, 0.f), 2.f * MATH::kPi,
           kCircleSegments, color);
    /* YZ */
    AddArc(lines, world, MATH::Vec3(), radius, MATH::Vec3(0.f, 1.f, 0.f), MATH::Vec3(0.f, 0.f, 1.f),
           2.f * MATH::kPi, kCircleSegments, color);
    /* ZX */
    AddArc(lines, world, MATH::Vec3(), radius, MATH::Vec3(0.f, 0.f, 1.f), MATH::Vec3(1.f, 0.f, 0.f),
           2.f * MATH::kPi, kCircleSegments, color);
}

void AddWireCylinder(DebugLines& lines, const MATH::Matrix4& world, const float radius, const float halfHeight,
                     const MATH::Vec3& color)
{
    /* Two Edge Circles */
    AddArc(lines, world, MATH::Vec3(0.f, halfHeight, 0.f), radius,
           MATH::Vec3(1.f, 0.f, 0.f), MATH::Vec3(0.f, 0.f, 1.f), 2.0f * MATH::kPi, kCircleSegments, color);
    AddArc(lines, world, MATH::Vec3(0.f, -halfHeight, 0.f), radius,
           MATH::Vec3(1.f, 0.f, 0.f), MATH::Vec3(0.f, 0.f, 1.f), 2.0f * MATH::kPi, kCircleSegments, color);

    /* Height Lines */
    AddLine(lines, world * MATH::Vec3(0.f, halfHeight, radius), world * MATH::Vec3(0.f, -halfHeight, radius), color);
    AddLine(lines, world * MATH::Vec3(0.f, halfHeight, -radius), world * MATH::Vec3(0.f, -halfHeight, -radius), color);
    AddLine(lines, world * MATH::Vec3(radius, halfHeight, 0.f), world * MATH::Vec3(radius, -halfHeight, 0.f), color);
    AddLine(lines, world * MATH::Vec3(-radius, halfHeight, 0.f), world * MATH::Vec3(-radius, -halfHeight, 0.f), color);
}

void AddWireCapsule(DebugLines& lines, const MATH::Matrix4& world, const float radius, const float halfHeight,
                    const MATH::Vec3& color)
{
    AddWireCylinder(lines, world, radius, halfHeight, color);
    /* Cap domes */
    AddArc(lines, world, MATH::Vec3(0.f, halfHeight, 0.f), radius, MATH::Vec3(1.f, 0.f, 0.f),
           MATH::Vec3(0.f, 1.f, 0.f), MATH::kPi,
            kCircleSegments / 2, color);
    AddArc(lines, world, MATH::Vec3(0.f, halfHeight, 0.f), radius, MATH::Vec3(0.f, 0.f, 1.f),
           MATH::Vec3(0.f, 1.f, 0.f), MATH::kPi,
           kCircleSegments / 2, color);

    AddArc(lines, world, MATH::Vec3(0.f, -halfHeight, 0.f), radius, MATH::Vec3(1.f, 0.f, 0.f),
           MATH::Vec3(0.f, -1.f, 0.f), MATH::kPi,
           kCircleSegments / 2, color);
    AddArc(lines, world, MATH::Vec3(0.f, -halfHeight, 0.f), radius, MATH::Vec3(0.f, 0.f, 1.f),
           MATH::Vec3(0.f, -1.f, 0.f), MATH::kPi,
           kCircleSegments / 2, color);
}
