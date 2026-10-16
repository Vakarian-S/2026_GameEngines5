#pragma once
#include <vulkan/vulkan_core.h>

struct TransitionMasks
{
    /** how the earlier work touches the image */
    VkAccessFlags2 srcAccessMask;
    /** how the later work touches the image */
    VkAccessFlags2 dstAccessMask;
    /** the pipeline stage where the earlier work happens */
    VkPipelineStageFlags2 srcStageMask;
    /** the pipeline stage where the later work happens */
    VkPipelineStageFlags2 dstStageMask;
};

/**
 * Records one layout change for one image.
 *
 * A layout is how the GPU arranges an image's memory for one kind of use. Drawing into an
 * image and presenting it need different layouts, so the image is switched between them.
 *
 * The four masks tell the GPU what to wait for. The src pair names the earlier work that
 * must finish before the change. The dst pair names the later work that must wait for it.
 *
 * @param commandBuffer Command Buffer to apply the barrier onto
 * @param image The image to change. A swapchain image, texture image, or the depth image
 * @param aspectMask which part of the image changes: VK_IMAGE_ASPECT_COLOR_BIT or VK_IMAGE_ASPECT_DEPTH_BIT
 * @param oldLayout the layout the image is in now. UNDEFINED means its contents may be thrown away
 * @param newLayout the layout the image ends up in
 * @param transitionMasks Masks for the image
 */
void RecordImageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask,
                        const VkImageLayout oldLayout,
                        const VkImageLayout newLayout, const TransitionMasks& transitionMasks);
