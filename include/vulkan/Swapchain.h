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

    /** @return the swapchain handle, which the acquire and present calls take */
    VkSwapchainKHR GetSwapchain() const { return swapChain; }

    /** @return the swapchain's images, in the order that the acquire call numbers them */
    const std::vector<VkImage>& GetImages() const { return images; }

    /** @return one view per swapchain image, in the same order as GetImages */
    const std::vector<VkImageView>& GetImageViews() const { return imageViews; }

    /** @return the pixel format of every swapchain image. A pipeline that draws into them needs it */
    VkFormat GetImageFormat() const { return imageFormat; }

    /** @return the size of every swapchain image in pixels */
    VkExtent2D GetExtent() const { return extent; }

    /** @return the depth image, which the layout change at the start of each frame takes */
    VkImage GetDepthImage() const { return depthImage; }

    /** @return the view of the depth image, which the depth attachment takes */
    VkImageView GetDepthImageView() const { return depthImageView; }

    /** @return the depth format. The pipeline needs it, so it must match */
    VkFormat GetDepthFormat() const { return depthFormat; }


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


    /* ========== Depth ========== */
    /* Dev Note: We have depth logic in here for convenience, since we can rebuild depth at the same time the swapchain
     * does, but now the "Swapchain" class owns something not related to the swapchain, this is a code smell
     * TODO: if this class gets another non swapchain feature, we have to migrate said feature and depth out of here crumbums */
    /** The depth image: one depth per pixel. Older APIs call it a "depth buffer", but in Vulkan it is a VkImage. */
    VkImage depthImage = VK_NULL_HANDLE;
    /** The view of the depth image. The depth attachment targets the view, never the image directly. */
    VkImageView depthImageView = VK_NULL_HANDLE;
    /** The memory behind the depth image. The swapchain's own images need none, because Vulkan allocates those. */
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    /** The format of the depth image. */
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    /** Builds the swapchain, its image views and the depth image from the window's current size. */
    void Create();

    /** Destroys the image views, the swapchain and the depth image. Create can run again afterwards. */
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
     * @param availableFormats the formats and colour spaces the window can show
     * @return the SRGB format when the list has it, or else the first format in the list
     */
    static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    /**
     * The swap extent is the resolution of the swap chain images,
     * and it’s almost always exactly equal to the resolution of the window that we’re drawing to in pixels
     * @param capabilities the smallest and biggest image sizes the surface allows
     * @param window the window, asked for its size in pixels
     * @return the window's size in pixels, kept inside the allowed sizes
     */
    static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window);
};
