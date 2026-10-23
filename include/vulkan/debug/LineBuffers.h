#pragma once
#include <array>
#include <cstdint>
#include <memory>

#include "core/debug/DebugLines.h"
#include "vulkan/GPUContext.h"

class Buffer;

/**
 * Class meant to hold the Vertex Buffers used in Debug Line Drawing
 */
class LineBuffers
{
public:
    explicit LineBuffers(const GPUContext& gpuContext_);

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    LineBuffers(const LineBuffers&) = delete;
    LineBuffers& operator=(const LineBuffers&) = delete;
    LineBuffers(LineBuffers&&) = delete;
    LineBuffers& operator=(LineBuffers&&) = delete;

    /**
     * Writes the lines given into the VkBuffer 
     * @param frameSlot Frame slot to use
     * @param lines Line data to put into the buffer
     */
    void Write(uint32_t frameSlot, const DebugLines& lines) const;

    /**
     * Returns the VkBuffer instance for the specific frame
     * @param frameSlot Frame slot of the buffer to return
     * @return The VkBuffer Instance for the given frame
     */
    [[nodiscard]] VkBuffer GetBuffer(uint32_t frameSlot) const;

    /** 32,768 lines, 1.5 MB per frame slot. */
    static constexpr uint32_t kMaxLineVertices = 65536;

private:
    /** Context to pass to the buffer creator. */
    const GPUContext& gpuContext;
    /** Unique buffers per frame to store the vertices */
    std::array<std::unique_ptr<Buffer>, kMaxFramesInFlight> buffers;
};
