#pragma once

/**
 * Struct containing the time since last step, which can be last frame or last fixed update
 */
struct DeltaTime {
    /** Time in seconds since last step */
    float seconds = 0.0f;
};
