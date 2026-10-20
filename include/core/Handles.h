#pragma once
#include <cstdint>

/**
 * A handle for a single mesh in the store
 */
struct MeshHandle
{
    uint32_t index;
};

/**
 * A handle for a single texture in the store
 */
struct TextureHandle
{
    uint32_t index;
};

/**
 * A handle for a single material in the store
 */
struct MaterialHandle
{
    uint32_t index;
};
