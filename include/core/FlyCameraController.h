#pragma once

#include "MMath.h"
#include "QMath.h"

union SDL_Event;

/**
 * A camera that flies: it moves along its own axes and the world's, and turns with yaw and pitch.
 * It stores a position and two angles, and rebuilds its rotation from them when needed, so turning never drifts.
 * dev-notes/fly-camera.md says why it stores no quaternion.
 */
class FlyCameraController
{
public:
    /**
     * @param eye where the camera starts
     * @param at the point it starts looking at. The world's up axis is always (0, 1, 0)
     */
    FlyCameraController(const MATH::Vec3& eye, const MATH::Vec3& at);

    /**
    * @param localMove metres along the camera's own axes: +X right, +Y up, -Z where it looks
    * @param worldMove metres along the world's axes, whichever way the camera faces
    */
    void Move(const MATH::Vec3& localMove, const MATH::Vec3& worldMove);

    /**
     * @param yawChangeDegrees turn about the world's up axis. Positive turns left
     * @param pitchChangeDegrees turn about the camera's side axis. Positive looks up. The total stops at 89 degrees either way
     */
    void Turn(float yawChangeDegrees, float pitchChangeDegrees);

    /** @return the view matrix, which moves the world so the camera sits at the origin looking down -Z */
    MATH::Matrix4 View() const;

    /**
     * Handles the events detected SDL_Events
     * @param event Event to be handled
     */
    void HandleEvent(const SDL_Event& event);

    /**
     * Update function to run every frame
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

private:
    /** @return the camera's rotation, built fresh from yaw and pitch */
    MATH::Quaternion Orientation() const;

    /** Where the camera is in the world */
    MATH::Vec3 position;
    /** The turn about the world's up axis. Kept between -180 and 180, so its sine and cosine stay precise */
    float yawDegrees = 0.0f;
    /** The tilt up or down. Kept between -89 and 89, so the camera never flips over the top */
    float pitchDegrees = 0.0f;
    /** Metres per second. Used from 1b.3 on */
    float moveSpeed = 5.0f;
    /** Degrees per pixel of mouse motion. Used from 1b.3 on */
    float mouseSensitivity = 0.1f;
    /** Boolean indicating whether the user is looking, usually by holding right click. **/
    bool looking = false;
};
