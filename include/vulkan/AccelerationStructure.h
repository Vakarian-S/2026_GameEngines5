#pragma once
#include "Buffer.h"

/**
 * One BLAS or TLAS: the buffer the driver lays the tree out in, its handle, and its GPU address.
 * Only create it when GPUContext::SupportsRayQuery is true.
 */
class AccelerationStructure
{
public:
    /**
     * Creates the storage buffer and the acceleration structure in it. It is empty until a build fills it.
     * @param gpuContext_ the device to create on. It must outlive this acceleration structure
     * @param deviceSize the size in bytes, from vkGetAccelerationStructureBuildSizesKHR
     * @param accelerationStructureType bottom level for a mesh, top level for the scene
     */
    AccelerationStructure(const GPUContext& gpuContext_, VkDeviceSize deviceSize,
                          VkAccelerationStructureTypeKHR accelerationStructureType);

    /** Destroys the handle. The storage buffer is destroyed after it, as a member. */
    ~AccelerationStructure();

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    AccelerationStructure(const AccelerationStructure&) = delete;
    AccelerationStructure& operator=(const AccelerationStructure&) = delete;
    AccelerationStructure(AccelerationStructure&&) = delete;
    AccelerationStructure& operator=(AccelerationStructure&&) = delete;

    /** @return the handle that builds write into and the shader's descriptor points at */
    VkAccelerationStructureKHR GetHandle() const { return handle; }

    /** @return the GPU address that a TLAS instance uses to point at this BLAS */
    VkDeviceAddress GetDeviceAddress() const { return deviceAddress; }

private:
    /** Where the create and destroy functions and the device come from. */
    const GPUContext& gpuContext;
    /** The device-local buffer the driver lays the tree out in. */
    Buffer storageBuffer;
    /** The acceleration structure itself. Null until the create succeeds. */
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    /** The GPU address, read once at creation because the TLAS fill reads it every frame. */
    VkDeviceAddress deviceAddress = 0;
};

/**
 * Builds one BLAS from a mesh's buffers and waits until the GPU has finished. Only call it when
 * GPUContext::SupportsRayQuery is true; both buffers need the device address and build input flags.
 * @param gpuContext the device to build on. It must outlive the BLAS
 * @param temporaryCommandBuffers records the build and waits for it
 * @param vertexBuffer the mesh's vertices. Positions are three floats at offset 0, sizeof(Vertex) apart
 * @param vertexCount how many vertices the buffer holds
 * @param indexBuffer the mesh's 32-bit indices. Every three make one triangle
 * @param indexCount how many indices the buffer holds
 * @return the built BLAS
 */
std::unique_ptr<AccelerationStructure> BuildBottomLevelAccelerationStructure(
    const GPUContext& gpuContext,
    const TemporaryCommandBuffers& temporaryCommandBuffers,
    const Buffer& vertexBuffer,
    std::uint32_t vertexCount,
    const Buffer& indexBuffer,
    std::uint32_t indexCount
);
