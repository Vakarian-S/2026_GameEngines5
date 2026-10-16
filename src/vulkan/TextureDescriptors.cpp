#include "vulkan/TextureDescriptors.h"

#include <stdexcept>

#include "vulkan/Texture.h"

TextureDescriptors::TextureDescriptors(const GPUContext& gpuContext_) : gpuContext(gpuContext_)
{
    CreateLayout();
    CreateDescriptorPool();
}

TextureDescriptors::~TextureDescriptors()
{
    /* Destroying the pool frees the sets allocated from it */
    vkDestroyDescriptorPool(gpuContext.GetDevice(), descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(gpuContext.GetDevice(), descriptorSetLayout, nullptr);
}

/* ============= 1. Set Layout ============= */
/* One binding: an image and the sampler that reads it, in one descriptor */
void TextureDescriptors::CreateLayout()
{
    VkDescriptorSetLayoutBinding layoutBinding{};
    /* The number the shader uses: layout (set = 1, binding = 0) */
    layoutBinding.binding = 0;
    /* A combined image sampler is an image view and a sampler together. The shader sees it as a sampler2D */
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBinding.descriptorCount = 1;
    /* Only the fragment shader samples the texture */
    layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &layoutBinding;

    if (vkCreateDescriptorSetLayout(gpuContext.GetDevice(), &descriptorSetLayoutCreateInfo, nullptr,
                                    &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create the texture descriptor set layout!");
    }
}

/* ============= 2. Pool ============= */
/* Room for kMaxTextures sets, each holding one combined image sampler */
void TextureDescriptors::CreateDescriptorPool()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = kMaxTextures;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = kMaxTextures;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(gpuContext.GetDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create the texture descriptor pool!");
    }
}

/* ============= 3. One Set Per Texture ============= */
/* Allocates the set, then points its binding 0 at the texture. The set never changes after this */
VkDescriptorSet TextureDescriptors::AllocateSet(const Texture& texture) const
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(gpuContext.GetDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate a texture descriptor set!");
    }

    /* Which view and sampler the descriptor points at, and the layout the image is in when the shader reads it */
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = texture.GetSampler();
    imageInfo.imageView = texture.GetImageView();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    /* Same write as in Descriptors, with pImageInfo in place of pBufferInfo */
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(gpuContext.GetDevice(), 1, &descriptorWrite, 0, nullptr);
    return descriptorSet;
}
