#include <utility>
#include <stdexcept>
#include <string>

#include "vulkan/debug/LineBuffers.h"
#include "vulkan/Buffer.h"

LineBuffers::LineBuffers(const GPUContext& gpuContext_) : gpuContext(gpuContext_)
{
    for (std::unique_ptr<Buffer>& buffer : buffers)
    {
        buffer = std::make_unique<Buffer>(gpuContext, sizeof(LineVertex) * kMaxLineVertices,
                                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void LineBuffers::Write(const uint32_t frameSlot, const DebugLines& lines) const
{
    if (lines.empty()) return;
    if (std::cmp_greater(lines.size(), kMaxLineVertices))
    {
        throw std::runtime_error("Too many lines for LineBuffers: " + std::to_string(lines.size()) +
            " lines, and the Lines buffer takes at most " + std::to_string(kMaxLineVertices));
    }
    buffers[frameSlot]->Write(lines.data(), lines.size() * sizeof(LineVertex));
}

VkBuffer LineBuffers::GetBuffer(const uint32_t frameSlot) const
{
    return buffers[frameSlot].get()->GetBuffer();
}
