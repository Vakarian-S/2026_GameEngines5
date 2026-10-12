#pragma once
#include <vulkan/vulkan_core.h>

#include "GPUContext.h"

class TemporaryCommandBuffers;

/**
 * One block of memory that Vulkan manages and the GPU reads.
 * The usage flags decide its job: vertex data, index data, or a staging copy.
 */
class Buffer
{
public:
    /**
      * Creates an empty buffer and the memory behind it.
      * @param gpuContext_ the device to build on. It must outlive this object
      * @param size_ the size of the buffer in bytes
      * @param usage_ what the buffer is for, such as VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
      * @param properties_ the kind of memory, such as host visible or device local
      */
    Buffer(const GPUContext& gpuContext_, VkDeviceSize size_, VkBufferUsageFlags usage_,
           VkMemoryPropertyFlags properties_);

    /**
     * Creates a buffer in GPU-only memory and fills it with data, through a staging buffer.
     * @param gpuContext_ the device to build on. It must outlive this object
     * @param temporaryCommandBuffers_ lends the command buffer that the copy from the staging buffer is recorded
     *        into. Only used inside this constructor
     * @param data_ the bytes to upload
     * @param size_ the number of bytes to upload
     * @param usage_ what the buffer is for, such as VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
     */
    Buffer(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_,
           const void* data_, VkDeviceSize size_, VkBufferUsageFlags usage_);


    /** Destroys the buffer, then frees its memory. */
    ~Buffer();

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;

    /** @return the buffer handle, which the bind and copy commands take */
    VkBuffer GetBuffer() const { return buffer; }
    /** @return the handle of the memory behind the buffer */
    VkDeviceMemory GetDeviceMemory() const { return deviceMemory; }

    /** @return the size of the buffer in bytes */
    VkDeviceSize GetSize() const { return size; }

    /**
     * Copies bytes from the CPU into the buffer. The buffer must use host visible memory.
     * @param data the bytes to copy
     * @param dataSize the number of bytes to copy
     */
    void Write(const void* data, VkDeviceSize dataSize) const;

private:
    /** Where the device comes from. */
    const GPUContext& gpuContext;
    /** The buffer itself. It has no memory until deviceMemory is bound to it. */
    VkBuffer buffer = VK_NULL_HANDLE;
    /** The memory behind the buffer. */
    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    /** The size of the buffer in bytes. */
    VkDeviceSize size = 0;
};
