#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

#include "GPUContext.h"

struct SDL_Window;

class Swapchain
{
public:
    /**
     * Builds the swapchain for one window.
     * @param gpuContext_ the device and surface to build on. It must outlive this object
     * @param window_ the window, asked for its size in pixels on every build
     */
    Swapchain(const GPUContext& gpuContext_, SDL_Window* window_);
    ~Swapchain();

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    VkSwapchainKHR GetSwapchain() const { return swapChain; }
    const std::vector<VkImage>& GetImages() const { return images; }
    const std::vector<VkImageView>& GetImageViews() const { return imageViews; }
    VkFormat GetImageFormat() const { return imageFormat; }
    VkExtent2D GetExtent() const { return extent; }

    /**
    * Rebuilds the swapchain at the window's current size. Call it after a resize.
    * @return false when the window has no area, as when minimized. Nothing is rebuilt then
    */
    bool Recreate();

private:
    /** Where the device, the surface and the queue families come from. */
    const GPUContext& gpuContext;

    /** The window this swapchain presents to. */
    SDL_Window* window = nullptr;

    /** The set of images that take turns on screen. */
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;

    /** The swapchain's images. The swapchain owns them, so Destroy leaves them alone. */
    std::vector<VkImage> images;

    /** One view per image. Drawing targets a view, never an image directly. */
    std::vector<VkImageView> imageViews;

    /** The pixel format of every image. */
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;

    /** The size of every image in pixels. */
    VkExtent2D extent{};

    /** Builds the swapchain and its image views from the window's current size. */
    void Create();

    /** Destroys the image views and the swapchain. Create can run again afterwards. */
    void Destroy();


    /**
     * Iterates through the present modes available and picks the one our program wants, if not, it falls back into
     * FIFO as every GPU has that mode
     * @param availablePresentModes list of availablePresentModes
     * @return Selected Present mode
     */
    static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    /**
     * Iterates through the surface formats available and picks the one our program wants we chose
     * For the color space we’ll use SRGB if it is available, because it results in more accurate perceived colors.
     * It is also pretty much the standard color space for images, like the textures we’ll use later on.
     * Because of that we should also use an SRGB color format, of which one of the most common ones is vk::Format::eB8G8R8A8Srgb.
     * IF not available we just pick the first one we find.
     * @param availableFormats 
     * @return 
     */
    static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    /**
     * The swap extent is the resolution of the swap chain images,
     * and it’s almost always exactly equal to the resolution of the window that we’re drawing to in pixels
     * @param capabilities
     * @param window 
     * @return 
     */
    static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window);
};
