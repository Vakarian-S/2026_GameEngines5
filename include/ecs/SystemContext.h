#pragma once
#include "ecs/World.h"

class CommandBuffer;

/**
 * Struct that contains the info that is passed to all system on run, It is meant to give them context of the scene
 * that they are running on, including the world instance and the command buffer to use
 */
struct SystemContext
{
    /* Warning Disabled because SystemContext is not meant to be reassigned */
    /** Reference to the world where this system is being used */
    World& world; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    /** Reference to the command buffer to use in this system */
    CommandBuffer& commands; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    /**
     * Gets resources from this world
     * @tparam T Type of Resource to get
     * @return Reference to the resource.
     */
    template <typename T>
    T& Get() const { return world.GetResources().Get<T>(); }
};
