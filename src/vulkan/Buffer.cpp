#include "vulkan/Buffer.h"
#include "vulkan/TemporaryCommandBuffers.h"
#include <cstring>

#include <stdexcept>
#include <utility>


Buffer::Buffer(const GPUContext& gpuContext_, const VkDeviceSize size_, const VkBufferUsageFlags usage_,
               const VkMemoryPropertyFlags properties_) : gpuContext(gpuContext_), size(size_) 
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size_;
    /*  is a bitmask of VkBufferUsageFlagBits specifying allowed usages of the buffer. */
    bufferInfo.usage = usage_;
    /* sharingMode is a VkSharingMode value specifying the sharing mode of the buffer when it will be accessed by multiple queue families.*/
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(gpuContext.GetDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer!");
    }

    /* Returns the memory requirements for specified Vulkan object */
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(gpuContext.GetDevice(), buffer, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    /*  index identifying a memory type from the memoryTypes array of the VkPhysicalDeviceMemoryProperties structure.*/
    memoryAllocateInfo.memoryTypeIndex = gpuContext.FindMemoryType(memoryRequirements.memoryTypeBits, properties_);

    if (vkAllocateMemory(gpuContext.GetDevice(), &memoryAllocateInfo, nullptr, &deviceMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate buffer memory!");
    }
    if (vkBindBufferMemory(gpuContext.GetDevice(), buffer, deviceMemory, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to bind buffer memory!");
    }
}

Buffer::Buffer(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_,
               const void* data_, const VkDeviceSize size_, const VkBufferUsageFlags usage_)
    : Buffer(gpuContext_, size_, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
{
    /* The CPU can write into the staging buffer. It is destroyed when this constructor returns */
    const Buffer stagingBuffer(gpuContext, size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer.Write(data_, size_);

    /* The GPU copies the staging buffer into this buffer. SubmitAndWait returns once the copy is done */
    const VkCommandBuffer commandBuffer = temporaryCommandBuffers_.BeginRecording();
    VkBufferCopy copyRegion{};
    copyRegion.size = size_;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer.GetBuffer(), buffer, 1, &copyRegion);
    temporaryCommandBuffers_.SubmitAndWait(commandBuffer);
}

Buffer::~Buffer()
{
    vkDestroyBuffer(gpuContext.GetDevice(), buffer, nullptr);
    vkFreeMemory(gpuContext.GetDevice(), deviceMemory, nullptr);
}

void Buffer::Write(const void* data, const VkDeviceSize dataSize) const
{
    /* Map gives the CPU an address for the buffer's memory. Unmap takes it away again */
    void* mapped = nullptr;
    if (vkMapMemory(gpuContext.GetDevice(), deviceMemory, 0, dataSize, 0, &mapped) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to map buffer memory!");
    }
    std::memcpy(mapped, data, dataSize);
    vkUnmapMemory(gpuContext.GetDevice(), deviceMemory);
}
