#pragma once
#include <vulkan/vulkan_core.h>

#include "Buffer.h"
#include "GPUContext.h"

class GPUContext;
class Texture;

/** The most materials the pool has room for. */
static constexpr uint32_t kMaxMaterials = 64;

/**
 * The surface values the fragment shader reads at set 1, binding 1. The uniform block in
 * phong.frag lists the same fields in the same order.
 */
struct MaterialUniform
{
    /** How tight the highlight is. Higher values make a smaller, sharper highlight. */
    float shininess;
    /** How bright the highlight is, from 0 for none to 1 for the light's full colour. */
    float specularStrength;
    /** Fills the struct to 16 bytes, the size the uniform buffer rules give the shader's block. */
    float padding[2];
};

static_assert(sizeof(MaterialUniform) == 16);


/**
 * Set 1: the material a draw uses. There is one set per material. Each points at the material's
 * texture and at its uniform buffer. A set is written once, when its material is created, and
 * never changes. So unlike set 0, it needs no copy per frame slot.
 */
class MaterialDescriptors
{
public:
    /**
     * Creates the set layout and the pool. No set exists until AllocateSet is called.
     * @param gpuContext_ the device to build on. It must outlive this object
     */
    explicit MaterialDescriptors(const GPUContext& gpuContext_);

    /** Destroys the pool, which frees every set allocated from it, then the set layout. */
    ~MaterialDescriptors();

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    MaterialDescriptors(const MaterialDescriptors&) = delete;
    MaterialDescriptors& operator=(const MaterialDescriptors&) = delete;
    MaterialDescriptors(MaterialDescriptors&&) = delete;
    MaterialDescriptors& operator=(MaterialDescriptors&&) = delete;

    /** @return the shape of a set, which the pipeline layout takes as set 1 */
    VkDescriptorSetLayout GetLayout() const { return descriptorSetLayout; }

    /**
     * Allocates one set from the pool and points it at a material's texture and uniform buffer.
     * @param texture the texture the set points at, as binding 0. It must outlive every draw that uses the set
     * @param materialBuffer the buffer holding a MaterialUniform, as binding 1. It must outlive every draw that uses the set
     * @return the set, which vkCmdBindDescriptorSets takes as set 1. The pool frees it, so the caller never does
     */
    VkDescriptorSet AllocateSet(const Texture& texture, const Buffer& materialBuffer) const;

private:
    /** The texture's binding number, which the shader uses as layout (set = 1, binding = 0). */
    static constexpr uint32_t kTextureBinding = 0;

    /** The material values' binding number, which the shader uses as layout (set = 1, binding = 1). */
    static constexpr uint32_t kMaterialBinding = 1;

    /** Where the device comes from. */
    const GPUContext& gpuContext;

    /** The shape of a set: a combined image sampler at binding 0 and a uniform buffer at binding 1, both read by the fragment shader. */
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    /** The memory that the sets are allocated from. Destroying it frees them. */
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    /** Creates the set layout. It describes a set and holds no data. */
    void CreateLayout();

    /** Creates the pool, with room for kMaxMaterials sets of two descriptors each. */
    void CreateDescriptorPool();
};
