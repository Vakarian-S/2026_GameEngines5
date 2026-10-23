#include "vulkan/PipelineSettings.h"
#include "vulkan/Vertex.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "core/debug/DebugLines.h"

PipelineSettings MeshPipelineSettings(const VkDescriptorSetLayout frameSet, const VkDescriptorSetLayout materialSet)
{
    const auto vertexAttributes = Vertex::GetVertexInputAttributeDescription();
    /* A fourth field in Vertex would not fit the fixed array, so the build stops here instead of writing past it */
    static_assert(std::tuple_size_v<decltype(vertexAttributes)> <= PipelineSettings::kMaxAttributes);

    /* Topology, cull mode and depth write keep their defaults, which are the mesh pipeline's values */
    PipelineSettings pipelineSettings{
        .vertexInput = {
            .bindingDescription = Vertex::GetVertexInputBindingDescription(),
            .attributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
        },
        .vertexShader = {.file = "shaders/phong.vert.spv"},
        .fragmentShader = {.file = "shaders/phong.frag.spv"},
        .layout = {
            /* Entry i describes set i of the shaders: set 0 the camera and the lights, set 1 the material */
            .setLayouts = {frameSet, materialSet},
            .setLayoutCount = 2,
            .pushConstantSize = sizeof(ModelMatrixPushConstant),
        },
    };

    /* The attributes come back as a std::array of their own size, so they are copied into the fixed slots */
    std::ranges::copy(vertexAttributes, pipelineSettings.vertexInput.attributeDescriptions.begin());
    return pipelineSettings;
}

PipelineSettings LinePipelineSettings(const VkDescriptorSetLayout frameSet)
{
    PipelineSettings pipelineSettings{
        .vertexInput = {
            /* One vertex buffer, at slot 0. The GPU steps sizeof(LineVertex) bytes from one vertex to the next */
            .bindingDescription = {
                .binding = 0,
                .stride = sizeof(LineVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            },
            /* Position at location 0 and colour at location 1, as debug.vert reads them. Both are three floats */
            .attributeDescriptions = {
                {
                    {
                        .location = 0,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32_SFLOAT,
                        .offset = offsetof(LineVertex, position),
                    },
                    {
                        .location = 1,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32_SFLOAT,
                        .offset = offsetof(LineVertex, color),
                    },
                }
            },
            .attributeDescriptionCount = 2,
        },
        .inputAssembly = {
            .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        },
        .vertexShader = {
            .file = "shaders/debug.vert.spv"
        },
        .rasterizer = {.cullMode = VK_CULL_MODE_NONE},
        .fragmentShader = {
            .file = "shaders/debug.frag.spv"
        },
        .depthTest = {.write = false},
        .layout = {
            .setLayouts = {frameSet},
            .setLayoutCount = 1,
        }
    };
    return pipelineSettings;
}
