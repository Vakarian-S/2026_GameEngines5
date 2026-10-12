#include "vulkan/TemporaryCommandBuffers.h"

#include <stdexcept>

TemporaryCommandBuffers::TemporaryCommandBuffers(const GPUContext& gpuContext_) : gpuContext(gpuContext_)
{
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = gpuContext.GetQueueFamilyIndices().graphicsFamily.value();
    /* Transient tells the driver that command buffers from this pool live for a short time */
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    if (vkCreateCommandPool(gpuContext.GetDevice(), &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create the temporary command pool!");
    }
}

TemporaryCommandBuffers::~TemporaryCommandBuffers()
{
    vkDestroyCommandPool(gpuContext.GetDevice(), commandPool, nullptr);
}

VkCommandBuffer TemporaryCommandBuffers::BeginRecording() const
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(gpuContext.GetDevice(), &commandBufferAllocateInfo, &commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate a temporary command buffer!");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    /* The command buffer is submitted once and then freed */
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin a temporary command buffer!");
    }
    return commandBuffer;
}

void TemporaryCommandBuffers::SubmitAndWait(const VkCommandBuffer commandBuffer) const
{
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to end a temporary command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    if (vkQueueSubmit(gpuContext.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit a temporary command buffer!");
    }

    /* Blocks until the GPU has run the instructions, so the caller can free what they used */
    vkQueueWaitIdle(gpuContext.GetGraphicsQueue());
    vkFreeCommandBuffers(gpuContext.GetDevice(), commandPool, 1, &commandBuffer);
}
