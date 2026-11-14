#include "vulkan/TopLevelAccelerationStructures.h"

#include "core/AlignUp.h"

TopLevelAccelerationStructures::TopLevelAccelerationStructures(const GPUContext& gpuContext_)
    : gpuContext(gpuContext_)
{
    if (!gpuContext.SupportsRayQuery()) return;

    for (Slot& slot : slots)
    {
        CreateSlot(slot, kStartingCapacity);
    }
}

void TopLevelAccelerationStructures::Fill(const uint32_t slotIndex, const DrawList& drawList,
                                          const ResourceStore& resourceStore)
{
    if (!gpuContext.SupportsRayQuery()) return;

    instances.clear();
    uint32_t capacity = slots[slotIndex].capacity;
    while (drawList.size() > capacity)
    {
        capacity *= 2;
    }
    if (capacity != slots[slotIndex].capacity) CreateSlot(slots[slotIndex], capacity);
    for (auto& drawItem : drawList)
    {
        const float* m = drawItem.transform;
        VkTransformMatrixKHR transform{};
        /* column major to row major */
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) transform.matrix[r][c] = m[c * 4 + r];

        VkAccelerationStructureInstanceKHR instance{};
        instance.transform = transform;
        instance.instanceCustomIndex = 0;
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = 0;
        instance.accelerationStructureReference = resourceStore.GetMesh(drawItem.mesh).blas->GetDeviceAddress();
        instances.push_back(instance);
    }
    slots[slotIndex].instanceCount = static_cast<uint32_t>(instances.size());
    if (instances.empty()) return;
    slots[slotIndex].instanceBuffer->Write(instances.data(),
                                           instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
}

void TopLevelAccelerationStructures::RecordBuild(VkCommandBuffer commandBuffer, uint32_t slotIndex)
{
    if (!gpuContext.SupportsRayQuery()) return;

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    /* VK_FALSE: the buffer holds the instances themselves, not pointers to them */
    instancesData.arrayOfPointers = VK_FALSE;
    instancesData.data.deviceAddress = slots[slotIndex].instanceBuffer->GetDeviceAddress();

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    accelerationStructureGeometry.geometry.instances = instancesData;

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
    accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    accelerationStructureBuildGeometryInfo.geometryCount = 1;
    accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
    accelerationStructureBuildGeometryInfo.dstAccelerationStructure = slots[slotIndex].accelerationStructure->
        GetHandle();

    const VkDeviceSize scratchAlignment = gpuContext.GetAccelerationStructureScratchAlignment();
    accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = AlignUp(
        slots[slotIndex].scratchBuffer->GetDeviceAddress(), scratchAlignment);

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = slots[slotIndex].instanceCount;
    const VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfos = &buildRangeInfo;

    const AccelerationStructureFunctions& functions = gpuContext.GetAccelerationStructureFunctions();
    functions.cmdBuildAccelerationStructures(commandBuffer, 1, &accelerationStructureBuildGeometryInfo,
                                             &buildRangeInfos);

    VkMemoryBarrier2 memoryBarrier2{};
    memoryBarrier2.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier2.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    memoryBarrier2.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    memoryBarrier2.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    memoryBarrier2.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pMemoryBarriers = &memoryBarrier2;
    dependencyInfo.memoryBarrierCount = 1; 
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void TopLevelAccelerationStructures::CreateSlot(Slot& slot, const uint32_t capacity)
{
    auto instanceBuffer = std::make_unique<Buffer>(
        gpuContext,
        capacity * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    /* VK_FALSE: the buffer holds the instances themselves, not pointers to them */
    instancesData.arrayOfPointers = VK_FALSE;

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    accelerationStructureGeometry.geometry.instances = instancesData;

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
    accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    accelerationStructureBuildGeometryInfo.geometryCount = 1;
    accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
    accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    const AccelerationStructureFunctions& functions = gpuContext.GetAccelerationStructureFunctions();
    functions.getBuildSizes(gpuContext.GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                            &accelerationStructureBuildGeometryInfo, &capacity,
                            &accelerationStructureBuildSizesInfo);

    auto tlas = std::make_unique<AccelerationStructure>(gpuContext,
                                                        accelerationStructureBuildSizesInfo.accelerationStructureSize,
                                                        VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);

    const VkDeviceSize scratchAlignment = gpuContext.GetAccelerationStructureScratchAlignment();
    auto scratchBuffer = std::make_unique<Buffer>(
        gpuContext, accelerationStructureBuildSizesInfo.buildScratchSize + scratchAlignment,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    slot.accelerationStructure = std::move(tlas);
    slot.instanceBuffer = std::move(instanceBuffer);
    slot.scratchBuffer = std::move(scratchBuffer);
    slot.capacity = capacity;
}
