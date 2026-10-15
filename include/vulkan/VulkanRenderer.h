#pragma once
#include "Descriptors.h"
#include "FrameLoop.h"
#include "GPUContext.h"
#include "Pipeline.h"
#include "ResourceStore.h"
#include "Swapchain.h"
#include "TemporaryCommandBuffers.h"


class VulkanRenderer
{
public:
    /**
     * Builds every Vulkan object the renderer needs, in the order Vulkan requires.
     * @param sdlWindow_ The window to draw into. It must outlive this object
     */
    explicit VulkanRenderer(SDL_Window* sdlWindow_);

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    VulkanRenderer& operator=(VulkanRenderer&&) = delete;
    /**
     * Draws one frame: every item in the list, seen through the camera.
     * @param camera the scene's camera. The projection is built from it here, for Vulkan
     * @param drawList what to draw this frame
     */
    void DrawFrame(const Camera& camera, const DrawList& drawList);


    ResourceStore& GetResourceStore() { return resourceStore; }

private:
    /** The colour every frame starts from, before anything is drawn. */
    static constexpr VkClearColorValue kClearColor{.float32 = {0.05f, 0.05f, 0.1f, 1.0f}};

    /** Where the device comes from. */
    GPUContext gpuContext;

    Descriptors descriptors;

    TemporaryCommandBuffers temporaryCommandBuffers;

    Swapchain swapchain;

    Pipeline pipeline;

    ResourceStore resourceStore;

    FrameLoop frameLoop;
};
