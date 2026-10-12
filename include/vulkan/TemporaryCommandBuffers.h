#pragma once
#include <vulkan/vulkan_core.h>

#include "GPUContext.h"

/**
 * Lends out a temporary command buffer, then runs it on the GPU right away and waits until the GPU is done.
 * Used for work done once at load time, such as copying data into a buffer or an image.
 */
class TemporaryCommandBuffers
{
public:
    /**
     * Creates the command pool that the temporary command buffers come from.
     * @param gpuContext_ the device and the graphics queue to work with. It must outlive this object
     */
    explicit TemporaryCommandBuffers(const GPUContext& gpuContext_);

    /** Destroys the command pool. */
    ~TemporaryCommandBuffers();

    /* Holds a Vulkan handle that the destructor destroys, so no copies or moves */
    TemporaryCommandBuffers(const TemporaryCommandBuffers&) = delete;
    TemporaryCommandBuffers& operator=(const TemporaryCommandBuffers&) = delete;
    TemporaryCommandBuffers(TemporaryCommandBuffers&&) = delete;
    TemporaryCommandBuffers& operator=(TemporaryCommandBuffers&&) = delete;

    /**
     * Hands out an empty command buffer that is ready to record into.
     * @return the command buffer. Pass it to SubmitAndWait when the instructions are recorded
     */
    VkCommandBuffer BeginRecording() const;

    /**
     * Sends the recorded instructions to the GPU, waits until the GPU has run them, then frees the command buffer.
     * @param commandBuffer the command buffer that BeginRecording handed out
     */
    void SubmitAndWait(VkCommandBuffer commandBuffer) const;

private:
    /** Where the device and the graphics queue come from. */
    const GPUContext& gpuContext;

    /** The memory that temporary command buffers are allocated from. Marked transient, because they live briefly. */
    VkCommandPool commandPool = VK_NULL_HANDLE;
};
