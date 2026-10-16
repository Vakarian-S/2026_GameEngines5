#pragma once
#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "GPUContext.h"

class GPUContext;
class Texture;

/** The most textures the pool has room for. */
static constexpr uint32_t kMaxTextures = 64;

/**
 * Set 1: the texture a draw samples. There is one set per texture, and each points at that
 * texture's view and sampler. A set is written once, when its texture is created, and never
 * changes. So unlike set 0, it needs no copy per frame slot.
 */
class TextureDescriptors
{
public:
    /**
     * Creates the set layout and the pool. No set exists until AllocateSet is called.
     * @param gpuContext_ the device to build on. It must outlive this object
     */
    explicit TextureDescriptors(const GPUContext& gpuContext_);

    /** Destroys the pool, which frees every set allocated from it, then the set layout. */
    ~TextureDescriptors();

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    TextureDescriptors(const TextureDescriptors&) = delete;
    TextureDescriptors& operator=(const TextureDescriptors&) = delete;
    TextureDescriptors(TextureDescriptors&&) = delete;
    TextureDescriptors& operator=(TextureDescriptors&&) = delete;

    /** @return the shape of a set, which the pipeline layout takes as set 1 */
    VkDescriptorSetLayout GetLayout() const { return descriptorSetLayout; }

    /**
    * Allocates one set from the pool and points it at a texture.
    * @param texture the texture the set points at. It must outlive every draw that uses the set
    * @return the set, which vkCmdBindDescriptorSets takes as set 1. The pool frees it, so the caller never does
    */
    VkDescriptorSet AllocateSet(const Texture& texture) const;

private:
    /** Where the device comes from. */
    const GPUContext& gpuContext;

    /** The shape of a set: one combined image sampler at binding 0, read by the fragment shader. */
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    /** The memory that the sets are allocated from. Destroying it frees them. */
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    /** Creates the set layout. It describes a set and holds no data. */
    void CreateLayout();

    /** Creates the pool, with room for kMaxTextures sets of one descriptor each. */
    void CreateDescriptorPool();
};
