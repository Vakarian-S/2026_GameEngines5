#include "vulkan/GPUQueries.h"

#include <stdexcept>

GPUQueries::GPUQueries(const GPUContext& gpuContext_) : gpuContext(gpuContext_)
{
    supported = gpuContext.SupportsTimestamps();
    if (!supported) return;

    timestampPeriod = gpuContext.GetTimestampPeriod();
    timestampMask = TimestampMask(gpuContext.GetTimestampValidBits());

    VkQueryPoolCreateInfo queryPoolCreateInfo{};
    queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolCreateInfo.queryCount = kQueryCount;

    for (VkQueryPool& pool : queryPools)
    {
        if (vkCreateQueryPool(gpuContext.GetDevice(), &queryPoolCreateInfo, nullptr, &pool) != VK_SUCCESS)
            throw std::runtime_error("failed to create timestamp query pool!");
    }
}

GPUQueries::~GPUQueries()
{
    for (const VkQueryPool pool : queryPools)
        vkDestroyQueryPool(gpuContext.GetDevice(), pool, nullptr);
}

void GPUQueries::RecordReset(const VkCommandBuffer commandBuffer, const uint32_t slot)
{
    if (!supported)
        return;

    currentSlot = slot;
    vkCmdResetQueryPool(commandBuffer, queryPools[slot], 0, kQueryCount);
    resetRecorded[slot] = true;
}

void GPUQueries::WriteTimestamp(const VkCommandBuffer commandBuffer, const uint32_t queryIndex,
                                const VkPipelineStageFlags2 stage) const
{
    if (!supported)
        return;

    vkCmdWriteTimestamp2(commandBuffer, stage, queryPools[currentSlot], queryIndex);
}

void GPUQueries::ReadResults(const uint32_t slot)
{
    if (!supported || !resetRecorded[slot])
        return;
    resetRecorded[slot] = false;

    std::array<uint64_t, kQueryCount> ticks{};
    const VkResult result = vkGetQueryPoolResults(gpuContext.GetDevice(), queryPools[slot], 0, kQueryCount,
                                                  sizeof(ticks), ticks.data(), sizeof(uint64_t),
                                                  VK_QUERY_RESULT_64_BIT);

    /* VK_NOT_READY means a timer was never written this frame, so the frame counts for nothing. */
    if (result != VK_SUCCESS)
        return;

    for (size_t i = 0; i < counts.size(); ++i)
    {
        sumsMilliseconds[i] += TimestampMilliseconds(ticks[i * 2], ticks[i * 2 + 1], timestampMask, timestampPeriod);
        ++counts[i];
    }
}

double GPUQueries::TakeAverageMilliseconds(const GPUTimerName name)
{
    const auto i = static_cast<size_t>(name);
    if (counts[i] == 0)
        return 0.0;

    const double average = sumsMilliseconds[i] / counts[i];
    sumsMilliseconds[i] = 0.0;
    counts[i] = 0;
    return average;
}

ScopedGPUTimer::ScopedGPUTimer(GPUQueries& gpuQueries_, const VkCommandBuffer commandBuffer_,
                               const GPUTimerName name)
    : gpuQueries(gpuQueries_), commandBuffer(commandBuffer_), startIndex(static_cast<uint32_t>(name) * 2)
{
    gpuQueries.WriteTimestamp(commandBuffer, startIndex, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
}

ScopedGPUTimer::~ScopedGPUTimer()
{
    gpuQueries.WriteTimestamp(commandBuffer, startIndex + 1, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
}
