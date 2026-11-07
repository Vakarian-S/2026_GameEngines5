#pragma once
#include "Descriptors.h"
#include "FrameLoop.h"
#include "GPUContext.h"
#include "Pipeline.h"
#include "ResourceStore.h"
#include "Swapchain.h"
#include "TemporaryCommandBuffers.h"
#include "MaterialDescriptors.h"
#include "core/DrawList.h"
#include "core/Light.h"
#include "vulkan/debug/LineBuffers.h"

enum class PresentMode : uint8_t;
struct Camera;


class VulkanRenderer
{
public:
    /**
     * Builds every Vulkan object the renderer needs, in the order Vulkan requires.
     * @param sdlWindow_ The window to draw into. It must outlive this object
     * @param startPresentMode desired present mode to use on the renderer at start.
     */
    explicit VulkanRenderer(SDL_Window* sdlWindow_, PresentMode startPresentMode);

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
    * @param debugLines the line vertices for this fram
    */
    void DrawFrame(const Camera& camera, const SceneLighting& lighting, const DrawList& drawList,
                   const DebugLines& debugLines);


    ResourceStore& GetResourceStore() { return resourceStore; }
    
    /**
     * A change from SetPresentMode shows here from the next frame
     * @return the present mode the swapchain uses now */
    PresentMode GetPresentMode() const;

    /**
     * Requests a rebuild from the swapchain and request using the given present mode
     * @param presentMode Present mode to use
     */
    void SetPresentMode(PresentMode presentMode);

    /**
     * Asks the swapchain if the GPU supports the given present mode
     * @param mode Present mode to check
     * @return true if the GPU supports the given mode
     */
    bool SupportsPresentMode(PresentMode mode) const;

    /**
     * Notify the FrameLoop that the swapchain needs a rebuild
     * that executes on the next DrawFrame
     */
    void RequestSwapchainRebuild();

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

    /** Pipeline used to draw the meshes */
    Pipeline meshPipeline;
    /** Pipeline used to draw the debug lines */
    Pipeline linePipeline;

    /** Line buffers that will be used for the linePipeline */
    LineBuffers lineBuffers;

    FrameLoop frameLoop;
};
