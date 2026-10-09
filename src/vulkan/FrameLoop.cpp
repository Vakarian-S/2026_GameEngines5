#include "vulkan/FrameLoop.h"

#include <cassert>
#include <stdexcept>

constexpr uint32_t kMaxFramesInFlight = 2;

FrameLoop::FrameLoop(const GPUContext& gpuContext_, Swapchain& swapchain_) : gpuContext(gpuContext_),
                                                                             swapchain(swapchain_)
{
    CreateCommandPool();
    CreateCommandBuffers();
    CreateSyncObjects();
}

FrameLoop::~FrameLoop()
{
    const VkDevice device = gpuContext.GetDevice();
    /* The GPU may still be running the last frames, and an object in use must not be destroyed */
    vkDeviceWaitIdle(device);

    /* Each vector has its own length, so each gets its own loop */
    for (const VkSemaphore semaphore : renderFinishedSemaphores) vkDestroySemaphore(device, semaphore, nullptr);
    for (const VkSemaphore semaphore : imageAvailableSemaphores) vkDestroySemaphore(device, semaphore, nullptr);
    for (const VkFence fence : inFlightFences) vkDestroyFence(device, fence, nullptr);

    /* Destroying the pool frees the command buffers allocated from it */
    vkDestroyCommandPool(device, commandPool, nullptr);
}

void FrameLoop::DrawFrame(const VkClearColorValue& clearColor)
{
    /**
    At a high level, rendering a frame in Vulkan consists of a common set of steps:
    - Wait for the previous frame to finish
    - Acquire an image from the swap chain
    - Record a command buffer which draws the scene onto that image
    - Submit the recorded command buffer
    - Present the swap chain image
    **/

    if (vkWaitForFences(gpuContext.GetDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to wait for fence!");
    }
    vkResetFences(gpuContext.GetDevice(), 1, &inFlightFences[currentFrame]);

    /* Index of the image currently available in our swapchain */
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(gpuContext.GetDevice(), swapchain.GetSwapchain(), UINT64_MAX,
                                            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    RecordCommandBuffer(imageIndex, clearColor);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const VkSemaphore imageAvailable[] = {imageAvailableSemaphores[currentFrame]};
    constexpr VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    /* waitSemaphoreCount is the number of semaphores upon which to wait before executing the command buffers for the batch. */
    submitInfo.waitSemaphoreCount = 1;
    /* is a pointer to an array of VkSemaphore handles upon which to wait before the command buffers for this batch begin execution. */
    submitInfo.pWaitSemaphores = imageAvailable;
    /* pWaitDstStageMask is a pointer to an array of pipeline stages at which each corresponding semaphore wait will occur. */
    submitInfo.pWaitDstStageMask = waitStages;

    /* commandBufferCount is the number of command buffers to execute in the batch */
    submitInfo.commandBufferCount = 1;
    /* pCommandBuffers is a pointer to an array of VkCommandBuffer handles to execute in the batch. */
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    const VkSemaphore renderFinished[] = {renderFinishedSemaphores[imageIndex]};
    /* signalSemaphoreCount is the number of semaphores to be signaled once the commands specified in pCommandBuffers have completed execution. */
    submitInfo.signalSemaphoreCount = 1;
    /* pSignalSemaphores is a pointer to an array of VkSemaphore handles which will be signaled when the command buffers for this batch have completed execution */
    submitInfo.pSignalSemaphores = renderFinished;


    if (vkQueueSubmit(gpuContext.GetGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    /* waitSemaphoreCount is the number of semaphores to wait for before issuing the present request. The number may be zero. */
    presentInfo.waitSemaphoreCount = 1;
    /* WaitSemaphores is NULL or a pointer to an array of VkSemaphore objects with waitSemaphoreCount entries,
     * and specifies the semaphores to wait for before issuing the present request.*/
    presentInfo.pWaitSemaphores = renderFinished;

    const VkSwapchainKHR swapChains[] = {swapchain.GetSwapchain()};
    /* swapchainCount is the number of swapchains being presented to by this command. */
    presentInfo.swapchainCount = 1;
    /* pSwapchains is a pointer to an array of VkSwapchainKHR objects with swapchainCount entries.*/
    presentInfo.pSwapchains = swapChains;
    /* pImageIndices is a pointer to an array of indices into the array of each swapchain’s presentable images, with swapchainCount entries.*/
    presentInfo.pImageIndices = &imageIndex;
    result = vkQueuePresentKHR(gpuContext.GetPresentQueue(), &presentInfo);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }
    currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
}


void FrameLoop::CreateCommandPool()
{
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = gpuContext.GetQueueFamilyIndices().graphicsFamily.value();
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(gpuContext.GetDevice(), &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool!");
    }
}

void FrameLoop::CreateCommandBuffers()
{
    commandBuffers.resize(kMaxFramesInFlight);
    VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.commandPool = commandPool;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocInfo.commandBufferCount = kMaxFramesInFlight;

    if (vkAllocateCommandBuffers(gpuContext.GetDevice(), &commandBufferAllocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void FrameLoop::CreateSyncObjects()
{
    assert(imageAvailableSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

    renderFinishedSemaphores.resize(swapchain.GetImages().size());
    imageAvailableSemaphores.resize(kMaxFramesInFlight);
    inFlightFences.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < swapchain.GetImages().size(); i++)
    {
        if (vkCreateSemaphore(gpuContext.GetDevice(), &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    for (size_t i = 0; i < kMaxFramesInFlight; i++)
    {
        if (vkCreateSemaphore(gpuContext.GetDevice(), &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[i]) !=
            VK_SUCCESS ||
            vkCreateFence(gpuContext.GetDevice(), &fenceCreateInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS
        )
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void FrameLoop::RecordCommandBuffer(const uint32_t imageIndex, const VkClearColorValue& clearColor) const
{
    const VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
    TransitionImageLayout(
        imageIndex,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        {}, // srcAccessMask (no need to wait for previous operations)
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, // dstAccessMask
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStage
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT // dstStage
    );
    // We set up the color attachment:
    VkRenderingAttachmentInfo attachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        /* imageView is the image view that will be used for rendering.*/
        .imageView = swapchain.GetImageViews()[static_cast<int>(imageIndex)],
        /* imageLayout is the layout that imageView will be in during rendering. */
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        /* Defines the load operation for the attachment. */
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        /* Defines the store operation for the attachment */
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = clearColor},
    };

    const VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        /* renderArea is the render area that is affected by the render pass instance */
        .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapchain.GetExtent()},
        /* layerCount is the number of layers rendered to in each attachment when viewMask is 0. */
        .layerCount = 1,
        /* colorAttachmentCount is the number of elements in pColorAttachments. */
        .colorAttachmentCount = 1,
        /* pColorAttachments is a pointer to an array of colorAttachmentCount VkRenderingAttachmentInfo structures describing any color attachments used. */
        .pColorAttachments = &attachmentInfo
    };
    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdEndRendering(commandBuffer);
    /* After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR */
    TransitionImageLayout(
        imageIndex,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        {}, // dstAccessMask
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStage
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT // dstStage
    );
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to end the command buffer!");
    }
}

void FrameLoop::TransitionImageLayout(const uint32_t imageIndex, const VkImageLayout oldLayout, const VkImageLayout newLayout,
                                      const VkAccessFlags2 srcAccessMask, const VkAccessFlags2 dstAccessMask,
                                      const VkPipelineStageFlags2 srcStageMask,
                                      const VkPipelineStageFlags2 dstStageMask) const
{
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapchain.GetImages()[static_cast<int>(imageIndex)],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    const VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };
    vkCmdPipelineBarrier2(commandBuffers[currentFrame], &dependencyInfo);
}
