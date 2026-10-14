#pragma once
#include "GPUContext.h"
#include "Matrix.h"
#include <array>
#include <memory>

class Buffer;
/**
 * The camera data that the vertex shader reads. The uniform block in the shader must list the
 * same fields in the same order.
 */
struct CameraStruct
{
    /** Moves the world so the camera sits at the origin. */
    MATH::Matrix4 view;
    /** Makes far things smaller, and maps depth into Vulkan's 0 to 1 range. */
    MATH::Matrix4 projection;
};

/**
 * A shader cannot read a buffer directly. It reads through a descriptor, which is a pointer to
 * a buffer. Descriptors are grouped into sets. There is one set and one buffer per frame slot,
 * because the GPU may still read one slot's buffer while the CPU writes the other.
 */
class Descriptors
{
public:
    /**
     * Creates the set layout, the pool, the sets, and the camera buffers.
     * @param gpuContext_ the device to build on. It must outlive this object
     */
    explicit Descriptors(const GPUContext& gpuContext_);

    /** Destroys the pool, which frees the sets, then the set layout. The buffers go after that. */
    ~Descriptors();

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    Descriptors(const Descriptors&) = delete;
    Descriptors& operator=(const Descriptors&) = delete;
    Descriptors(Descriptors&&) = delete;
    Descriptors& operator=(Descriptors&&) = delete;

    /** @return the shape of a set, which the pipeline layout takes */
    VkDescriptorSetLayout GetLayout() const { return descriptorSetLayout; }

    /**
    * @param frameSlot which frame slot is being recorded
    * @return the set that points at that slot's camera buffer, which vkCmdBindDescriptorSets takes
    */
    VkDescriptorSet GetSet(const uint32_t frameSlot) const { return descriptorSets[frameSlot]; }

    /**
     * Copies the camera data into one frame slot's buffer.
     * @param frameSlot which frame slot to write. The GPU must have finished that slot's last frame
     * @param camera the view and projection matrices for this frame
     */
    void WriteCamera(uint32_t frameSlot, const CameraStruct& camera) const;

private:
    /** Where the device comes from. */
    const GPUContext& gpuContext;

    /** The shape of a set: binding 0 is one uniform buffer, read by the vertex shader. It holds no data. */
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    /** The memory that the sets are allocated from. Destroying it frees them. */
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    /** One set per frame slot. A set holds pointers, not data: set i points at cameraBuffers[i]. */
    std::array<VkDescriptorSet, kMaxFramesInFlight> descriptorSets{};

    /** One camera buffer per frame slot, in memory the CPU can write. */
    std::array<std::unique_ptr<Buffer>, kMaxFramesInFlight> cameraBuffers;

    /** Creates the set layout. It describes a set and holds no data. */
    void CreateLayout();

    /** Creates the pool, with room for one set and one uniform buffer descriptor per frame slot. */
    void CreateDescriptorPool();

    /** Allocates one set per frame slot from the pool. The sets point at nothing yet. */
    void CreateDescriptorSets();

    /** Creates one camera buffer per frame slot. */
    void CreateCameraBuffers();

    /** Points set i at camera buffer i. CreateDescriptorSets and CreateCameraBuffers must run first. */
    void PointSetsAtBuffers() const;
};
