#pragma once
#include <cstdint>

constexpr uint16_t kInvalidArchetype = 0xFFFF;

struct EntitySlot
{
    uint32_t version;
    /* Archetype type: 0 = Alive Empty Archetype, 0xFFFF = Free field */
    uint16_t archetype;
    uint32_t row;

    EntitySlot() : version(0), archetype(kInvalidArchetype), row(0)
    {
    }

    EntitySlot(const uint32_t version, const uint16_t archetype, const uint32_t row) : version(version), archetype(archetype), row(row)
    {
    }
};
