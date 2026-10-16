#include "vulkan/Texture.h"

#include <stb_image.h>

#include "core/Paths.h"
#include "vulkan/Buffer.h"
#include "vulkan/ImageBarrier.h"

Texture::Texture(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_,
                 const std::string& path_) : gpuContext(gpuContext_)
{
    /* ============= 1. Load ============= */
    /* The last argument asks for 4 channels, so every file arrives as RGBA. fileChannels says what the file held */
    int fileWidth = 0;
    int fileHeight = 0;
    int fileChannels = 0;
    stbi_uc* pixels = stbi_load(PATHS::FromExecutable(path_).c_str(), &fileWidth, &fileHeight, &fileChannels,
                                STBI_rgb_alpha);
    if (pixels == nullptr)
    {
        throw std::runtime_error("failed to load texture " + path_ + ": " + stbi_failure_reason());
    }
    width = static_cast<uint32_t>(fileWidth);
    height = static_cast<uint32_t>(fileHeight);
    /* 4 bytes per pixel, because 4 channels were asked for, whatever the file held */
    const VkDeviceSize size = static_cast<VkDeviceSize>(width) * height * 4;

    /* ============= 2. Staging ============= */
    /* The CPU writes the pixels here. The CPU copy is freed at once, and the staging buffer lives until this constructor returns */
    const Buffer stagingBuffer(gpuContext, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer.Write(pixels, size);
    stbi_image_free(pixels);

    /* ============= 3. Image, Upload, View, Sampler ============= */
    CreateImage();
    UploadPixels(temporaryCommandBuffers_, stagingBuffer);
    CreateImageView();
    CreateSampler();
}

Texture::~Texture()
{
    vkDestroySampler(gpuContext.GetDevice(), sampler, nullptr);
    vkDestroyImageView(gpuContext.GetDevice(), imageView, nullptr);
    vkDestroyImage(gpuContext.GetDevice(), image, nullptr);
    vkFreeMemory(gpuContext.GetDevice(), imageMemory, nullptr);
}

void Texture::CreateImage()
{
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = kFormat;
    imageCreateInfo.extent = {.width = width, .height = height, .depth = 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    /* The copy writes into it, then the shader samples it */
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(gpuContext.GetDevice(), &imageCreateInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create the texture image!");
    }

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(gpuContext.GetDevice(), image, &memoryRequirements);
    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = gpuContext.FindMemoryType(memoryRequirements.memoryTypeBits,
                                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(gpuContext.GetDevice(), &memoryAllocateInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate the texture image memory!");
    }
    if (vkBindImageMemory(gpuContext.GetDevice(), image, imageMemory, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to bind the texture image memory!");
    }
}

void Texture::UploadPixels(const TemporaryCommandBuffers& temporaryCommandBuffers, const Buffer& stagingBuffer) const
{
    const VkCommandBuffer commandBuffer = temporaryCommandBuffers.BeginRecording();

    /* The image becomes something a copy can write into. It holds nothing yet, so there is no earlier work to wait for */
    RecordImageBarrier(commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       {
                           .srcAccessMask = VK_ACCESS_2_NONE,
                           .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                           .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                           .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                       });

    /* The whole buffer goes into the whole image. A row length of 0 means the rows sit tight, with no gap between them */
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {.x = 0, .y = 0, .z = 0};
    region.imageExtent = {.width = width, .height = height, .depth = 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.GetBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region);

    /* The image becomes something the fragment shader can sample. The shader waits for the copy to finish */
    RecordImageBarrier(commandBuffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       {
                           .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                           .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                           .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                           .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                       });

    /* Returns once the GPU has run all three commands, so the staging buffer may be destroyed after this */
    temporaryCommandBuffers.SubmitAndWait(commandBuffer);
}

void Texture::CreateImageView()
{
    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = kFormat;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(gpuContext.GetDevice(), &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create the texture image view!");
    }
}

void Texture::CreateSampler()
{
    /* The GPU's limits include the highest anisotropy it supports */
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(gpuContext.GetPhysicalDevice(), &properties);

    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    /* Linear blends the four nearest pixels, for both a close-up (mag) and a far view (min) */
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    /* Past the edge of the image, the image repeats */
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    /* Anisotropy keeps the image sharp on surfaces seen at a steep angle. GPUContext enables the feature */
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    /* Coordinates run from 0 to 1 across the image, not from 0 to the width in pixels */
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    /* Comparing is for shadow maps. Off here */
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    /* One mip level, so the mip settings change nothing */
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    if (vkCreateSampler(gpuContext.GetDevice(), &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create the texture sampler!");
    }
}
