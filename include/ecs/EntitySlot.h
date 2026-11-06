#pragma once
#include <cstdint>

constexpr uint16_t kInvalidArchetype = 0xFFFF;

/** Maximum int number used in world to indicate there are no recycable entity slots and you are going to have to create a new one */
constexpr uint32_t kNoFreeSlot = 0xFFFFFFFF;

/**
 * An entity slot contains the information of a given entity to be able to access it, if the slot is not occupied
 * by an entity then it gives information about the next recyclable entity slot
 */
struct EntitySlot
{
    /** Expected version of the pointed entity, if versions dont match, it means the entity is destroyed */
    uint32_t version;
    /** Archetype type: 0 = Alive Empty Archetype, 0xFFFF = Free field */
    uint16_t archetype;
    /** Row inside its archetype where the entity lives, if its the invalid archetype then it means the next empty
     * entity slot for recycling */
    uint32_t row;

    EntitySlot() : version(0), archetype(kInvalidArchetype), row(0)
    {
    }

    EntitySlot(const uint32_t version, const uint16_t archetype, const uint32_t row) : version(version), archetype(archetype), row(row)
    {
    }
};
