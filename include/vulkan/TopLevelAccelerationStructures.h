#pragma once
#include <array>
#include <vector>

#include "AccelerationStructure.h"
#include "GPUContext.h"
#include "ResourceStore.h"
#include "core/DrawList.h"

/**
 * One TLAS per frame slot, each with its own instance buffer and scratch buffer. The CPU fills one
 * slot while the GPU may still read the other. Every slot stays empty when ray query is off.
 */
class TopLevelAccelerationStructures
{
public:
    /**
     * Creates every slot at kStartingCapacity. Does nothing when ray query is off.
     * @param gpuContext_ the device to create on. It must outlive this object
     */
    explicit TopLevelAccelerationStructures(const GPUContext& gpuContext_);

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    TopLevelAccelerationStructures(const TopLevelAccelerationStructures&) = delete;
    TopLevelAccelerationStructures& operator=(const TopLevelAccelerationStructures&) = delete;
    TopLevelAccelerationStructures(TopLevelAccelerationStructures&&) = delete;
    TopLevelAccelerationStructures& operator=(TopLevelAccelerationStructures&&) = delete;

    /**
     * Fills the Acceleration Structure buffer with the information from the drawlist.
     * @param slotIndex frame slot to use
     * @param drawList the draw list to write into the buffer
     * @param resourceStore resource store to read for mesh info
     */
    void Fill(uint32_t slotIndex, const DrawList& drawList, const ResourceStore& resourceStore);

    /**
     * Records the build of the TLAS in the given command buffer, meant to be used on the Record section of the draw loop
     * @param commandBuffer command buffer to use
     * @param slotIndex frame slot index to use
     */
    void RecordBuild(VkCommandBuffer commandBuffer, uint32_t slotIndex);

    /**
     * @param slotIndex frame index to use
     * @return The handle of the TLAS for this frame, VK_NULL_HANDLE if no ray query available
     */
    VkAccelerationStructureKHR GetHandle(const uint32_t slotIndex) const
    {
        const std::unique_ptr<AccelerationStructure>& tlas = slots[slotIndex].accelerationStructure;
        return tlas ? tlas->GetHandle() : VK_NULL_HANDLE;
    }

private:
    /** Where the device, the functions and the scratch alignment come from. */
    const GPUContext& gpuContext;

    /** How many instances each slot holds at start. Small, so Scene0's 1,000 cubes grow it on the first frame. */
    static constexpr uint32_t kStartingCapacity = 256;

    /** Everything one frame slot needs to build its TLAS. All null when ray query is off. */
    struct Slot
    {
        /** The slot's TLAS. Set 0 points at it from step 6 on. */
        std::unique_ptr<AccelerationStructure> accelerationStructure;
        /** One VkAccelerationStructureInstanceKHR per drawn object. The CPU writes it every frame. */
        std::unique_ptr<Buffer> instanceBuffer;
        /** The build's working memory, reused every frame. One alignment bigger than the build asks for. */
        std::unique_ptr<Buffer> scratchBuffer;
        /** How many instances the three were sized for. */
        uint32_t capacity = 0;
        /** How many instances this frame wrote, which the build reads. */
        uint32_t instanceCount = 0;
    };

    /** One slot per frame in flight, picked by FrameLoop's currentFrame. */
    std::array<Slot, kMaxFramesInFlight> slots;
    /** vector of AccelerationStructureInstances created from the draw list, stored to be wrote all at once */
    std::vector<VkAccelerationStructureInstanceKHR> instances;

    /**
     * Creates a slot's instance buffer, TLAS and scratch buffer for a number of instances. Replaces
     * what the slot held before, so only call it once that slot's fence has passed.
     * @param slot the slot to fill
     * @param capacity how many instances it must hold
     */
    void CreateSlot(Slot& slot, uint32_t capacity);
};
