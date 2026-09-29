#pragma once
#include <cstdint>

/* Set on the index of a handle naming an entity a command buffer has not created yet. */
constexpr uint32_t kPlaceholderBit = 0x80000000;

struct Entity
{
    /* Index of the entity on the */ 
    uint32_t index;
    uint32_t version;
};
