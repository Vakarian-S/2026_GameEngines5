#pragma once
#include <array>
#include <vulkan/vulkan_core.h>

#include "GPUContext.h"
#include "core/TimestampMath.h"
#include "utils/Profiler.h"

/** Names each GPU timer. Count sizes the query pools, at two timestamps per name. */
enum class GPUTimerName : uint8_t
{
    MeshPass,
    Count
};

/** Two timestamp queries per GPUTimerName: the start, then the end. */
constexpr uint32_t kQueryCount = static_cast<uint32_t>(GPUTimerName::Count) * 2;

/**
 * Owns one timestamp query pool per frame slot, records their resets and writes,
 * reads a slot after its fence wait, and averages each timer in milliseconds.
 */
class GPUQueries
{
public:
    explicit GPUQueries(const GPUContext& gpuContext_);
    ~GPUQueries();

    /* GPUQueries owns its query pools, so a copy would destroy them twice. */
    GPUQueries(const GPUQueries&) = delete;
    GPUQueries(GPUQueries&&) = delete;
    GPUQueries& operator=(const GPUQueries&) = delete;
    GPUQueries& operator=(GPUQueries&&) = delete;

    /**
     * Records the reset of one slot's pool. Goes first in the command buffer, outside any rendering block.
     * @param commandBuffer command buffer used 
     * @param slot frame slot to use
     */
    void RecordReset(VkCommandBuffer commandBuffer, uint32_t slot);

    /**
     * Records one timestamp write into the pool of the slot that BeginFrame named.
     * @param commandBuffer command buffer to use
     * @param queryIndex index of the query
     * @param stage Pipeline stage to record
     */
    void WriteTimestamp(VkCommandBuffer commandBuffer, uint32_t queryIndex, VkPipelineStageFlags2 stage) const;

    /**
     * Reads one slot's timestamps and adds them to the averages. Call right after that slot's fence wait.
     * @param slot Frame slot to use
     */
    void ReadResults(uint32_t slot);

    /** @return one timer's average in ms since the last call, then clears it. 0 when nothing was measured */
    double TakeAverageMilliseconds(GPUTimerName name);

private:
    /** gpu context used for these queries */
    const GPUContext& gpuContext;
    /** False when the GPU cannot write timestamps. Then every method returns at once. */
    bool supported = false;
    /** Pools that the query instances use, one per frame slot */
    std::array<VkQueryPool, kMaxFramesInFlight> queryPools{};
    /** True once a slot's pool was reset, so ReadResults never reads an unused pool. */
    std::array<bool, kMaxFramesInFlight> resetRecorded{};
    /** The slot BeginFrame last named. WriteTimestamp writes into its pool. */
    uint32_t currentSlot = 0;
    /** Nanoseconds per timestamp tick. */
    float timestampPeriod = 0.0f;
    /** Keeps only the timestamp bits that the graphics queue writes. */
    uint64_t timestampMask = 0;
    /** Sum of the total milliseconds stored across the readings in one frame */
    std::array<double, static_cast<size_t>(GPUTimerName::Count)> sumsMilliseconds{};
    /** Count of the operations recorded in said phase */
    std::array<uint32_t, static_cast<size_t>(GPUTimerName::Count)> counts{};
};

/** Records a start timestamp when created and an end timestamp when destroyed, for one GPUTimerName. */
class ScopedGPUTimer
{
public:
    ScopedGPUTimer(GPUQueries& gpuQueries_, VkCommandBuffer commandBuffer_, GPUTimerName name);
    ~ScopedGPUTimer();

    /* One scope, one timer. A copy would record the same scope twice. */
    ScopedGPUTimer(const ScopedGPUTimer&) = delete;
    ScopedGPUTimer(ScopedGPUTimer&&) = delete;
    ScopedGPUTimer& operator=(const ScopedGPUTimer&) = delete;
    ScopedGPUTimer& operator=(ScopedGPUTimer&&) = delete;

private:
    GPUQueries& gpuQueries;
    VkCommandBuffer commandBuffer;

    /** The start query. The end query is the next one. */
    uint32_t startIndex;
};

/* ============================== Macros ============================== */

#if GE5_PROFILING

/** Times the enclosing block of GPU commands under one GPUTimerName */
#define PROFILE_GPU_SCOPE(queries, commandBuffer, name) \
const ScopedGPUTimer GE5_PROFILE_JOIN(gpuTimer, __LINE__)(queries, commandBuffer, name)

#else

/** Profiling off - does nothing */
#define PROFILE_GPU_SCOPE(queries, commandBuffer, name) ((void)0)

#endif
