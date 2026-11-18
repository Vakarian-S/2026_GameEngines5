#include "vulkan/ImageBarrier.h"

void RecordImageBarrier(const VkCommandBuffer commandBuffer, const VkImage image, const VkImageAspectFlags aspectMask,
                        const VkImageLayout oldLayout,
                        const VkImageLayout newLayout, const TransitionMasks& transitionMasks, const uint32_t layerCount)
{
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = transitionMasks.srcStageMask,
        .srcAccessMask = transitionMasks.srcAccessMask,
        .dstStageMask = transitionMasks.dstStageMask,
        .dstAccessMask = transitionMasks.dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = layerCount
        }
    };
    const VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}
