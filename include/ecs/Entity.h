#pragma once
#include <cstdint>

/* Set on the index of a handle naming an entity a command buffer has not created yet. */
constexpr uint32_t kPlaceholderBit = 0x80000000;

/**
 * World::CreateEntity gives a version of 1 or more. A default Entity{} has version 0, which means no entity
 */
struct Entity
{
    /** Index of the Entity Slot in the world that holds this entity */ 
    uint32_t index;
    /** Current version of the entity, when the entity gets destroyed its version number its increased */
    uint32_t version;
};
