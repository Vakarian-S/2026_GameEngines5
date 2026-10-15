#pragma once
#include "Matrix.h"

/**
 * Gameplay Camera struct.
 */
struct Camera
{
    /** Moves the world so the camera sits at the origin, looking down -Z. */
    MATH::Matrix4 view;
    /** How much the camera sees from bottom to top, in degrees. */
    float fovYDegrees = 45.0f;
    /** Nothing nearer than this is drawn. */
    float nearPlane = 0.1f;
    /** Nothing farther than this is drawn. */
    float farPlane = 100.0f;
};
