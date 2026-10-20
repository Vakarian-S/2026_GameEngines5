#include "vulkan/MaterialDescriptors.h"

#include <array>
#include <stdexcept>

#include "vulkan/Buffer.h"
#include "vulkan/Texture.h"

MaterialDescriptors::MaterialDescriptors(const GPUContext& gpuContext_) : gpuContext(gpuContext_)
{
    CreateLayout();
    CreateDescriptorPool();
}

MaterialDescriptors::~MaterialDescriptors()
{
    /* Destroying the pool frees the sets allocated from it */
    vkDestroyDescriptorPool(gpuContext.GetDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(gpuContext.GetDevice(), descriptorSetLayout, nullptr);
}

/* ============= 1. Set Layout ============= */
/* Two bindings: the texture with the sampler that reads it, and the uniform buffer with the surface values */
void MaterialDescriptors::CreateLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> layoutBindings{};

    layoutBindings[0].binding = kTextureBinding;
    /* A combined image sampler is an image view and a sampler together. The shader sees it as a sampler2D */
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBindings[0].descriptorCount = 1;
    /* Only the fragment shader samples the texture */
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    layoutBindings[1].binding = kMaterialBinding;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[1].descriptorCount = 1;
    /* Only the fragment shader computes the highlight */
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    descriptorSetLayoutCreateInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(gpuContext.GetDevice(), &descriptorSetLayoutCreateInfo, nullptr,
                                    &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create the material descriptor set layout!");
    }
}

/* ============= 2. Pool ============= */
/* Room for kMaxMaterials sets. A pool counts descriptors per type, so each type gets its own entry */
void MaterialDescriptors::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    /* One texture per set */
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = kMaxMaterials;
    /* One uniform buffer per set */
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = kMaxMaterials;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    /* maxSets caps the set objects, whatever they hold. The sizes above cap the descriptors inside them */
    poolInfo.maxSets = kMaxMaterials;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(gpuContext.GetDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create the material descriptor pool!");
    }
}

/* ============= 3. One Set Per Material ============= */
/* Allocates the set, then points binding 0 at the texture and binding 1 at the buffer. The set never changes after this */
VkDescriptorSet MaterialDescriptors::AllocateSet(const Texture& texture, const Buffer& materialBuffer) const
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(gpuContext.GetDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate a material descriptor set!");
    }

    /* Which view and sampler binding 0 points at, and the layout the image is in when the shader reads it */
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = texture.GetSampler();
    imageInfo.imageView = texture.GetImageView();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    /* Which buffer, and which bytes of it, binding 1 points at */
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = materialBuffer.GetBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(MaterialUniform);

    /* The writes point into imageInfo and bufferInfo, so both must live until vkUpdateDescriptorSets returns */
    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = kTextureBinding;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &imageInfo;

    /* Same write as in Descriptors, with pBufferInfo in place of pImageInfo */
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = kMaterialBinding;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInfo;

    /* One call fills both bindings */
    vkUpdateDescriptorSets(gpuContext.GetDevice(), static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
    return descriptorSet;
}
