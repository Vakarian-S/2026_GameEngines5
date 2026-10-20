#pragma once
#include <span>

#include "Vector.h"

/** One point light. It shines the same in every direction from one place. */
struct Light
{
    /** Where the light sits, in world space. */
    MATH::Vec3 position;
    /** The light's colour and brightness, as red, green, and blue. 1 is full strength. */
    MATH::Vec3 color;
};

/** Everything that lights a scene this frame. The renderer copies it into a uniform buffer. */
struct SceneLighting
{
    /** Light that reaches every surface, so a side facing away from every light is not black. */
    MATH::Vec3 ambient;
    /** The point lights. The span points into the scene's own storage, which must live until DrawFrame returns. */
    std::span<const Light> lights;
};
