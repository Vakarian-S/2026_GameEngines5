#include "core/FlyCameraController.h"
#include "ConstantsConversions.h"

#include <algorithm>
#include <cmath>
#include <SDL3/SDL.h>

FlyCameraController::FlyCameraController(const MATH::Vec3& eye, const MATH::Vec3& at) :
    position(eye)
{
    /* Yaw 0 looks down -Z, as lookAt does, and yaw +90 looks down -X */
    const MATH::Vec3 direction = MATH::VMath::normalize(at - eye);
    yawDegrees = std::atan2(-direction.x, -direction.z) * RADIANS_TO_DEGREES;
    pitchDegrees = std::asin(direction.y) * RADIANS_TO_DEGREES;
}

MATH::Quaternion FlyCameraController::Orientation() const
{
    /* Yaw on the left turns about the world's up axis. Pitch on the right turns about the camera's own side axis */
    return MATH::QMath::angleAxisRotation(yawDegrees, MATH::Vec3(0.0f, 1.0f, 0.0f)) *
        MATH::QMath::angleAxisRotation(pitchDegrees, MATH::Vec3(1.0f, 0.0f, 0.0f));
}

void FlyCameraController::Move(const MATH::Vec3& localMove, const MATH::Vec3& worldMove)
{
    /* The local move is turned to face where the camera looks. The world move is added as it is */
    position += MATH::QMath::rotate(localMove, Orientation()) + worldMove;
}

void FlyCameraController::Turn(const float yawChangeDegrees, const float pitchChangeDegrees)
{
    /* Wrapped into -180 to 180, because the sine of a large float angle loses precision */
    yawDegrees = std::remainder(yawDegrees + yawChangeDegrees, 360.0f);
    pitchDegrees = std::clamp(pitchDegrees + pitchChangeDegrees, -89.0f, 89.0f);
}

MATH::Matrix4 FlyCameraController::View() const
{
    /* The camera rotates, then moves to position. The view undoes both in reverse: move back, then rotate back */
    return MATH::MMath::toMatrix4(MATH::QMath::conjugate(Orientation())) * MATH::MMath::translate(-position);
}

void FlyCameraController::HandleEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT)
    {
        looking = true;
        SDL_SetWindowRelativeMouseMode(SDL_GetWindowFromID(event.button.windowID), true);
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
    {
        looking = false;
        SDL_SetWindowRelativeMouseMode(SDL_GetWindowFromID(event.button.windowID), false);
    }
    if (looking && event.type == SDL_EVENT_MOUSE_MOTION)
    {
        const float yawChangeDegrees = -event.motion.xrel * mouseSensitivity;
        const float pitchChangeDegrees = -event.motion.yrel * mouseSensitivity;
        Turn(yawChangeDegrees, pitchChangeDegrees);
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
    {
        looking = false;
        SDL_SetWindowRelativeMouseMode(SDL_GetWindowFromID(event.window.windowID), false);
    }
}

void FlyCameraController::Update(const float deltaTime)
{
    /* SDL keeps this array up to date while events are pulled. Scancodes name the key's place, not its letter */
    const bool* keys = SDL_GetKeyboardState(nullptr);

    /* The camera looks down -Z, so W is -Z */
    MATH::Vec3 localDirection;
    if (keys[SDL_SCANCODE_W]) localDirection.z -= 1.0f;
    if (keys[SDL_SCANCODE_S]) localDirection.z += 1.0f;
    if (keys[SDL_SCANCODE_A]) localDirection.x -= 1.0f;
    if (keys[SDL_SCANCODE_D]) localDirection.x += 1.0f;

    MATH::Vec3 worldDirection;
    if (keys[SDL_SCANCODE_E]) worldDirection.y += 1.0f;
    if (keys[SDL_SCANCODE_Q]) worldDirection.y -= 1.0f;

    if (MATH::VMath::mag(localDirection) > 0.0f)
    {
        localDirection = MATH::VMath::normalize(localDirection);
    }

    const float speed = moveSpeed * (keys[SDL_SCANCODE_LSHIFT] ? 4.0f : 1.0f) * deltaTime;
    Move(localDirection * speed, worldDirection * speed);
}
