#include "vulkan/AccelerationStructure.h"
#include <stdexcept>

#include "vulkan/TemporaryCommandBuffers.h"
#include "core/AlignUp.h"
#include "vulkan/Vertex.h"

AccelerationStructure::AccelerationStructure(
    const GPUContext& gpuContext_,
    const VkDeviceSize deviceSize,
    const VkAccelerationStructureTypeKHR accelerationStructureType
)
    : gpuContext(gpuContext_),
      storageBuffer(
          gpuContext_,
          deviceSize,
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
{
    VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
    accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelerationStructureCreateInfo.buffer = storageBuffer.GetBuffer();
    accelerationStructureCreateInfo.size = deviceSize;
    accelerationStructureCreateInfo.type = accelerationStructureType;

    const AccelerationStructureFunctions& functions = gpuContext.GetAccelerationStructureFunctions();
    if (functions.createAccelerationStructure(gpuContext.GetDevice(), &accelerationStructureCreateInfo, nullptr,
                                              &handle) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create acceleration structure!");
    }

    VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
    accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    accelerationStructureDeviceAddressInfo.accelerationStructure = handle;

    deviceAddress = functions.getDeviceAddress(gpuContext.GetDevice(), &accelerationStructureDeviceAddressInfo);
}

AccelerationStructure::~AccelerationStructure()
{
    gpuContext.GetAccelerationStructureFunctions().
               destroyAccelerationStructure(gpuContext.GetDevice(), handle, nullptr);
}

std::unique_ptr<AccelerationStructure> BuildBottomLevelAccelerationStructure(
    const GPUContext& gpuContext,
    const TemporaryCommandBuffers& temporaryCommandBuffers,
    const Buffer& vertexBuffer,
    std::uint32_t vertexCount,
    const Buffer& indexBuffer,
    std::uint32_t indexCount)
{
    VkAccelerationStructureGeometryTrianglesDataKHR accelerationStructureGeometryTrianglesData{};
    accelerationStructureGeometryTrianglesData.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    accelerationStructureGeometryTrianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    accelerationStructureGeometryTrianglesData.vertexData.deviceAddress = vertexBuffer.GetDeviceAddress();
    accelerationStructureGeometryTrianglesData.vertexStride = sizeof(Vertex);
    accelerationStructureGeometryTrianglesData.maxVertex = vertexCount - 1;
    accelerationStructureGeometryTrianglesData.indexType = VK_INDEX_TYPE_UINT32;
    accelerationStructureGeometryTrianglesData.indexData.deviceAddress = indexBuffer.GetDeviceAddress();


    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    accelerationStructureGeometry.geometry.triangles = accelerationStructureGeometryTrianglesData;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
    accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    accelerationStructureBuildGeometryInfo.geometryCount = 1;
    accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    /* The driver needs the triangle count to size the tree and the scratch memory */
    const std::uint32_t triangleCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
    accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    const AccelerationStructureFunctions& functions = gpuContext.GetAccelerationStructureFunctions();
    functions.getBuildSizes(gpuContext.GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                            &accelerationStructureBuildGeometryInfo, &triangleCount,
                            &accelerationStructureBuildSizesInfo);

    auto blas = std::make_unique<AccelerationStructure>(gpuContext,
                                                        accelerationStructureBuildSizesInfo.accelerationStructureSize,
                                                        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);

    accelerationStructureBuildGeometryInfo.dstAccelerationStructure = blas->GetHandle();

    /* A fresh buffer's address may miss the alignment, so the buffer gets one alignment extra and the address rounds up */
    const VkDeviceSize scratchAlignment = gpuContext.GetAccelerationStructureScratchAlignment();
    const Buffer scratchBuffer(gpuContext, accelerationStructureBuildSizesInfo.buildScratchSize + scratchAlignment,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    accelerationStructureBuildGeometryInfo.scratchData.deviceAddress =
        AlignUp(scratchBuffer.GetDeviceAddress(), scratchAlignment);

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = triangleCount;
    /* The build command takes one array of ranges per build, so it wants a pointer to this pointer */
    const VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos = &buildRangeInfo;

    /* SubmitAndWait returns once the build is done, so the scratch buffer can die at the closing brace */
    const VkCommandBuffer commandBuffer = temporaryCommandBuffers.BeginRecording();
    functions.cmdBuildAccelerationStructures(commandBuffer, 1, &accelerationStructureBuildGeometryInfo,
                                             &buildRangeInfos);
    temporaryCommandBuffers.SubmitAndWait(commandBuffer);

    return blas;
}
