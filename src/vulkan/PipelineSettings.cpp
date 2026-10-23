#include "vulkan/PipelineSettings.h"
#include "vulkan/Vertex.h"

#include <algorithm>
#include <array>

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
