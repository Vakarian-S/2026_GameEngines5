#include "vulkan/Swapchain.h"

#include <algorithm>
#include <cassert>
#include <SDL3/SDL_video.h>
#include <limits>
#include <stdexcept>
#include <iostream>

Swapchain::Swapchain(const GPUContext& gpuContext_, SDL_Window* window_) : gpuContext(gpuContext_), window(window_)
{
    Create();
}

Swapchain::~Swapchain()
{
    Destroy();
}

void Swapchain::Create()
{
    const SwapChainSupportDetails swapChainSupportDetails = gpuContext.QuerySwapChainSupport(
        gpuContext.GetPhysicalDevice());
    const auto surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupportDetails.formats);
    const auto presentMode = ChooseSwapPresentMode(swapChainSupportDetails.presentModes);
    const auto swapExtent = ChooseSwapExtent(swapChainSupportDetails.capabilities, window);
    QueueFamilyIndices queueFamilyIndices = gpuContext.GetQueueFamilyIndices();

    /**
     * Sticking to a minimum of 1 means that we may sometimes have to wait on the driver to complete internal operations
     * before we can acquire another image to render to. In practice, it’s common to request a few more images than
     * the strict minimum to give yourself some headroom - the code below requests at least three images,
     * while also making sure to not exceed the maximum number of images the implementation supports,
     * where 0 is a special value that means there is no maximum:
     */
    auto minImageCount = std::max(3u, swapChainSupportDetails.capabilities.minImageCount);
    if (0 < swapChainSupportDetails.capabilities.maxImageCount && swapChainSupportDetails.capabilities.maxImageCount
        < minImageCount)
    {
        minImageCount = swapChainSupportDetails.capabilities.maxImageCount;
    }

    /** 
     * Image Sharing: Exclusive is faster as it requires you to hand over queues manually
     * Image Sharing: Concurrent is slightly slower but it saves you the trouble of handing out queues in a multiple family queue
     */
    const uint32_t queueFamilyIndicesList[] = {
        queueFamilyIndices.graphicsFamily.value(),
        queueFamilyIndices.presentFamily.value()
    };

    VkSharingMode imageSharingMode;
    uint32_t queueFamilyIndexCount = 0;
    const uint32_t* pQueueFamilyIndices = nullptr;

    if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily)
    {
        imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        queueFamilyIndexCount = 2;
        pQueueFamilyIndices = queueFamilyIndicesList;
    }
    else
    {
        imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    const VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        /* is the surface onto which the swapchain will present images. If the creation succeeds, the swapchain becomes associated with surface. */
        .surface = gpuContext.GetSurface(),
        /* is the minimum number of presentable images that the application needs.
         * The implementation will either create the swapchain with at least that many images, or it will fail to create the swapchain. */
        .minImageCount = minImageCount,
        /* Is a VkFormat value specifying the format the swapchain image(s) will be created with. */
        .imageFormat = surfaceFormat.format,
        /* Is a VkColorSpaceKHR value specifying the way the swapchain interprets image data.*/
        .imageColorSpace = surfaceFormat.colorSpace,
        /* s the size (in pixels) of the swapchain image(s).
         * The behavior is platform-dependent if the image extent does not match the surface’s currentExtent as returned
         * by vkGetPhysicalDeviceSurfaceCapabilitiesKHR.*/
        .imageExtent = swapExtent,
        /* Is the number of views in a multiview/stereo surface. For non-stereoscopic-3D applications, this value is 1.*/
        .imageArrayLayers = 1,
        /* Is a bitmask of VkImageUsageFlagBits describing the intended usage of the (acquired) swapchain images.*/
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        /* Is the sharing mode used for the image(s) of the swapchain. */
        .imageSharingMode = imageSharingMode,
        /* is the number of queue families having access to the image(s) of the swapchain when imageSharingMode is VK_SHARING_MODE_CONCURRENT. */
        .queueFamilyIndexCount = queueFamilyIndexCount,
        /* Is a pointer to an array of queue family indices having access to the image(s) of the swapchain when imageSharingMode is VK_SHARING_MODE_CONCURRENT. */
        .pQueueFamilyIndices = pQueueFamilyIndices,
        /* Is a VkSurfaceTransformFlagBitsKHR value describing the transform,
         * relative to the presentation engine’s natural orientation, applied to the
         * image content prior to presentation. */
        .preTransform = swapChainSupportDetails.capabilities.currentTransform,
        /*  is a VkCompositeAlphaFlagBitsKHR value indicating the alpha compositing mode to use when this surface
         *  is composited together with other surfaces on certain window systems. */
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        /* Is the presentation mode the swapchain will use. A swapchain’s present mode determines how incoming present requests will be processed and queued internally. */
        .presentMode = presentMode,
        /* specifies whether the Vulkan implementation is allowed to discard rendering operations that affect regions of the surface that are not visible. */
        .clipped = true
    };

    if (vkCreateSwapchainKHR(gpuContext.GetDevice(), &swapchainCreateInfo, nullptr, &swapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    /* Vulkan may make more images than the minimum asked for, so ask how many it made */
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(gpuContext.GetDevice(), swapChain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(gpuContext.GetDevice(), swapChain, &imageCount, images.data());

    imageFormat = surfaceFormat.format;
    extent = swapExtent;

    /* One view per image. A view tells Vulkan how to read an image: as 2D colour, one layer, no mipmaps */
    imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        /* image is a VkImage on which the view will be created. */
        imageViewCreateInfo.image = images[i];
        /* viewType is a VkImageViewType value specifying the type of the image view. */
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        /* format is a VkFormat specifying the format and type used to interpret texel blocks of the image. */
        imageViewCreateInfo.format = imageFormat;
        /* Is a VkImageSubresourceRange structure selecting the set of mipmap levels and array layers to be accessible to the view. */
        /* aspectMask is a bitmask of VkImageAspectFlagBits specifying which aspect(s) of the image are included in the view. */
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        /* baseMipLevel is the first mipmap level accessible to the view. */
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        /* levelCount is the number of mipmap levels (starting from baseMipLevel) accessible to the view. */
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        /* baseArrayLayer is the first array layer accessible to the view. */
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        /* layerCount is the number of array layers (starting from baseArrayLayer) accessible to the view. */
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(gpuContext.GetDevice(), &imageViewCreateInfo, nullptr, &imageViews[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create a swapchain image view!");
        }
    }

    std::cout << "Swapchain: " << imageCount << " images, format " << imageFormat << ", "
        << extent.width << "x" << extent.height << std::endl;
}

void Swapchain::Destroy()
{
    /* Views first, then the swapchain. The swapchain owns the images, so nothing destroys those here */
    for (const VkImageView imageView : imageViews)
    {
        vkDestroyImageView(gpuContext.GetDevice(), imageView, nullptr);
    }
    imageViews.clear();
    images.clear();

    if (swapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(gpuContext.GetDevice(), swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }
}

VkPresentModeKHR Swapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& mode : availablePresentModes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    assert(!availableFormats.empty());
    for (const auto& format : availableFormats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }
    return availableFormats[0];
}

VkExtent2D Swapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window)
{
    /* Any width other than the largest uint32_t means the surface has fixed the size, so use it as given */
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    /* The surface lets the program decide, so ask SDL for the window's size in real pixels */
    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(window, &width, &height))
    {
        throw std::runtime_error("failed to get the window size in pixels!");
    }

    return {
        .width = std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        .height = std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}

bool Swapchain::Recreate()
{
    /* A minimized window has no area, and Vulkan cannot build a swapchain of size zero */
    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpuContext.GetPhysicalDevice(), gpuContext.GetSurface(), &capabilities);
    const bool minimized = (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0;
    if (minimized || capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0) return false;

    /* The GPU may still be drawing into the old images */
    vkDeviceWaitIdle(gpuContext.GetDevice());
    Destroy();
    Create();
    return true;
}
