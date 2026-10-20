#pragma once

#include "Descriptors.h"
#include "GPUContext.h"
#include "ResourceStore.h"
#include "Swapchain.h"

class Pipeline;
class Buffer;
class Descriptors;
/**
 * Draws one frame per call.
 *
 * The constructor sets up the tools: a command pool, one command buffer per frame slot, and the
 * semaphores and fences that keep the CPU and the GPU in step.
 *
 * DrawFrame then runs every pass of the main loop:
 *   1. Wait for the fence of this frame slot, which the frame from two passes ago signals.
 *   2. Ask the swapchain for a free image.
 *   3. Record the commands for this frame into the slot's command buffer.
 *   4. Submit the command buffer to the graphics queue.
 *   5. Ask the present queue to show the image.
 *
 * Steps 4 and 5 only hand work over. The GPU does it later, in order, while the CPU moves on.
 */
class FrameLoop
{
public:
    /**
     * Creates the command pool, the command buffers, and the sync objects.
     * @param gpuContext_ The device and queues to work with. It must outlive this object
     * @param swapchain_ The images to draw into. It must outlive this object
     * @param descriptors_ The camera sets and buffers, one per frame slot. It must outlive this object
     */
    FrameLoop(const GPUContext& gpuContext_, Swapchain& swapchain_, const Descriptors& descriptors_);

    /** Waits for the GPU to finish, then destroys the sync objects and the command pool. */
    ~FrameLoop();

    /**
     * Draws one frame: clears the next swapchain image to one colour, draws with the pipeline, and presents it.
     * @param clearColor the colour of the image is cleared to before anything is drawn
     * @param pipeline the settings to draw with
     * @param resourceStore where the meshes named in drawList live
     * @param drawList what to draw: one mesh and one model matrix per item
     * @param camera the view and projection for this frame, copied into this slot's camera buffer
     * @param lights the lights for this frame, copied into this slot's light buffer
     */
    void DrawFrame(const VkClearColorValue& clearColor, const Pipeline& pipeline, const ResourceStore& resourceStore,
                   const DrawList& drawList, const CameraUniform& camera, const LightUniform& lights);

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    FrameLoop(const FrameLoop&) = delete;
    FrameLoop& operator=(const FrameLoop&) = delete;
    FrameLoop(FrameLoop&&) = delete;
    FrameLoop& operator=(FrameLoop&&) = delete;

private:
    /** Where the device and the two queues come from. */
    const GPUContext& gpuContext;

    /** The images this loop draws into and presents. */
    Swapchain& swapchain;

    /** The camera sets and buffers. DrawFrame writes this slot's camera and binds this slot's set. */
    const Descriptors& descriptors;

    /**
     * The memory that command buffers record into. A buffer reuses this memory every frame, so
     * recording allocates nothing new. Destroying the pool frees its command buffers.
     */
    VkCommandPool commandPool = VK_NULL_HANDLE;

    /**
     * Which frame slot this pass uses. It picks the command buffer, the fence, and the
     * imageAvailable semaphore. It has no link to the swapchain image index.
     */
    uint32_t currentFrame = 0;

    /** Set when Vulkan reports that the swapchain no longer fits the window. The next DrawFrame rebuilds it. */
    bool swapchainNeedsRebuild = false;

    /**
     * One list of recorded commands per frame slot. The CPU records into it, and the GPU plays
     * it back after the submit. There are two, because a buffer the GPU is still playing must
     * not be recorded again.
     */
    std::vector<VkCommandBuffer> commandBuffers;

    /**
     * "The image is free now." One per frame slot.
     * Signalled by: the acquire, once the screen has let go of the image.
     * Waited on by: the GPU, before it starts drawing.
     * Without it, the GPU could draw into an image that is still on screen.
     */
    std::vector<VkSemaphore> imageAvailableSemaphores;

    /**
     * "The drawing is done." One per swapchain image.
     * Signalled by: the GPU, when it finishes this frame's commands.
     * Waited on by: the present.
     * Without it, the screen could show a half-drawn image.
     */
    std::vector<VkSemaphore> renderFinishedSemaphores;

    /**
     * "This slot's commands are finished." One per frame slot.
     * In flight means submitted to the GPU and not finished yet.
     * Signalled by: the GPU, when it finishes this frame's commands.
     * Waited on by: the CPU, at the top of DrawFrame.
     * Without it, the CPU could record over a command buffer the GPU is still reading.
     */
    std::vector<VkFence> inFlightFences;

    /** Creates the command pool for the graphics queue family, with single buffer reset allowed. */
    void CreateCommandPool();

    /** Allocates one command buffer per frame slot from the pool. */
    void CreateCommandBuffers();

    /**
     * Creates the semaphores and the fences. The fences start signalled, so the first wait on
     * each slot returns at once.
     */
    void CreateSyncObjects();

    /** Creates one render finished semaphore per swapchain image. */
    void CreateRenderFinishedSemaphores();

    /** Destroys every render finished semaphore. */
    void DestroyRenderFinishedSemaphores();

    /**
     * Rebuilds the swapchain and the semaphores that belong to its images.
     * @return false when the window is minimized, so the caller skips the frame
     */
    bool RebuildSwapchain();

    /**
     * Records this frame's commands into the current slot's command buffer:
     *   1. Change the image's layout so the GPU can draw into it.
     *   2. Begin rendering, which clears the image to clearColor.
     *   3. Bind the pipeline and both buffers, set the viewport and scissor, and draw.
     *   4. End rendering.
     *   5. Change the image's layout so it can be presented.
     *
     * @param imageIndex which swapchain image to draw into
     * @param clearColor the color the image is cleared to
     * @param pipeline the settings to draw with
     * @param resourceStore where the meshes named in drawList live
     * @param drawList what to draw: one mesh and one model matrix per item
     */
    void RecordCommandBuffer(uint32_t imageIndex, const VkClearColorValue& clearColor, const Pipeline& pipeline,
                             const ResourceStore& resourceStore, const DrawList& drawList) const;
};
