#pragma once
#include "Descriptors.h"
#include "FrameLoop.h"
#include "GPUContext.h"
#include "Pipeline.h"
#include "ResourceStore.h"
#include "Swapchain.h"
#include "TemporaryCommandBuffers.h"
#include "MaterialDescriptors.h"


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
    * @param lighting the scene's lights. More than kMaxLights throws
    * @param drawList what to draw this frame
    */
    void DrawFrame(const Camera& camera, const SceneLighting& lighting, const DrawList& drawList);


    ResourceStore& GetResourceStore() { return resourceStore; }

private:
    /** The colour every frame starts from, before anything is drawn. */
    static constexpr VkClearColorValue kClearColor{.float32 = {0.05f, 0.05f, 0.1f, 1.0f}};

    /** Where the device comes from. */
    GPUContext gpuContext;

    /** Set 0: the camera, one set and one buffer per frame slot. */
    Descriptors descriptors;

    /** Set 1: one set per material. Declared before resourceStore, which allocates from it */
    MaterialDescriptors materialDescriptors;

    TemporaryCommandBuffers temporaryCommandBuffers;

    Swapchain swapchain;

    /** Every mesh and texture. Declared before pipeline only for reading order; the pipeline does not use it */
    ResourceStore resourceStore;

    Pipeline meshPipeline;

    FrameLoop frameLoop;
};
