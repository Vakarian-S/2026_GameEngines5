#pragma once

#include <array>
#include <SDL3/SDL_scancode.h>

#include "Vector.h"

/**
 * This Struct holds information about the current user input, it is meant to be the bridge between 
 * input API's like SDL and our ECS system
 */
struct InputState
{
    /** Keys currently pressed right now */
    std::array<bool, SDL_SCANCODE_COUNT> held{};
    /** Keys Pressed down during this frame, read in Update only; Fixed may miss it or count it twice */
    std::array<bool, SDL_SCANCODE_COUNT> pressed{};
    /** Position of the mouse in Pixel Space in this frame */
    MATH::Vec2 mousePosition;
    /** Mouse buttons currently pressed this frame indexed by SDL_BUTTON_LEFT */
    std::array<bool, 6> mousePressed{};
    /** Size of the window this frame, in window units */
    MATH::Vec2 windowSize;

    /**
     * Checks whether a key corresponding to the specified SDL_Scancode is currently being held down.
     *
     * @param code The SDL_Scancode representing the key to check.
     * @return True if the specified key is currently held down, false otherwise.
     */
    bool Held(const SDL_Scancode code) const
    {
        return held[code];
    }

    /**
     * Checks if a key corresponding to the specified SDL_Scancode is currently pressed.
     *
     * @param code The SDL_Scancode representing the key to check.
     * @return True if the specified key is currently pressed, false otherwise.
     */
    bool Pressed(const SDL_Scancode code) const
    {
        return pressed[code];
    }
};
