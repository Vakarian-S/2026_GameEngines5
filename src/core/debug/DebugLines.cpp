#include "core/debug/DebugLines.h"

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
