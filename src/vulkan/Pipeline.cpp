#include "vulkan/Pipeline.h"
#include "vulkan/Vertex.h"

#include <fstream>
#include <stdexcept>

#include "core/Paths.h"


Pipeline::Pipeline(const GPUContext& gpuContext_, const VkFormat colorFormat_, const VkFormat depthFormat_,
                   const std::string& vertFile_,
                   const std::string& fragFile_, const std::span<const VkDescriptorSetLayout> descriptorSetLayouts_) :
    gpuContext(gpuContext_), colorFormat(colorFormat_), depthFormat(depthFormat_)
{
    CreatePipelineLayout(descriptorSetLayouts_);
    CreateGraphicsPipeline(vertFile_, fragFile_);
}

Pipeline::~Pipeline()
{
    vkDestroyPipeline(gpuContext.GetDevice(), pipeline, nullptr);
    vkDestroyPipelineLayout(gpuContext.GetDevice(), pipelineLayout, nullptr);
}

std::vector<char> Pipeline::ReadFile(const std::string& filename)
{
    const std::string filepathFromExecutable = PATHS::FromExecutable(filename);
    std::ifstream file(filepathFromExecutable, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + filepathFromExecutable);
    }
    const size_t fileSize = file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule Pipeline::CreateShaderModule(const std::vector<char>& code) const
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    /*  is the size, in bytes, of the code pointed to by pCode.*/
    shaderModuleCreateInfo.codeSize = code.size();
    /*  pointer to code that is used to create the shader module. */
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule shaderModule;

    if (vkCreateShaderModule(gpuContext.GetDevice(), &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

void Pipeline::CreateGraphicsPipeline(const std::string& vertFile, const std::string& fragFile)
{
    /*
     * The sections below follow the order of the graphics pipeline, from vertex data to the
     * finished pixel. Each one fills the settings for one step.
     */

    /* ============= 1. Vertex Input ============= */
    /* How the bytes of a vertex buffer are laid out: the size of one vertex, and where each field sits in it */
    const VkVertexInputBindingDescription bindingDescription = Vertex::GetVertexInputBindingDescription();
    const auto attributeDescriptions = Vertex::GetVertexInputAttributeDescription();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    /* ============= 2. Input Assembly ============= */
    /* How vertices join into shapes. Triangle list means every three vertices make one triangle */
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    /* defines the primitive topology */
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    /* primitiveRestartEnable controls whether a special vertex index value is treated as restarting the assembly of primitives. */
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    /* ============= 3. Vertex Shader ============= */
    /* Runs once per vertex and gives each one its position on screen */
    const std::vector<char> vertShaderCode = ReadFile(vertFile);
    const VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    /* module is optionally a VkShaderModule object containing the shader code for this stage */
    vertShaderStageInfo.module = vertShaderModule;
    /* pName is a pointer to a null-terminated UTF-8 string specifying the entry point name of the shader for this stage. */
    vertShaderStageInfo.pName = "main";

    /* ============= 4. Tessellation and Geometry Shaders ============= */
    /* Both steps are optional and this pipeline skips them, so they have no settings */

    /* ============= 5. Viewport and Scissor ============= */
    /* One viewport and one scissor. FrameLoop sets their sizes every frame, so no values here */
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    /* ============= 6. Rasterizer ============= */
    /* How a triangle becomes pixels. Back faces are skipped, and counter clockwise corners mark the front */
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
    rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    /* depthClampEnable controls whether to clamp the fragment's depth values as described in Depth Test */
    rasterizerCreateInfo.depthClampEnable = VK_FALSE;
    /*  controls whether primitives are discarded immediately before the rasterization stage. */
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    /* is the triangle rendering mode */
    rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    /*  is the width of rasterized line segments.*/
    rasterizerCreateInfo.lineWidth = 1.0f;
    /*  is the triangle facing direction used for primitive culling */
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    /*  is a VkFrontFace value specifying the front-facing triangle orientation to be used for culling. */
    rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    /* controls whether to bias fragment depth values. */
    rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

    /* ============= 7. Multisample ============= */
    /* One sample per pixel, which means no anti-aliasing */
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo{};
    multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
    /* is a VkSampleCountFlagBits value specifying the number of samples used in rasterization */
    multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    /* ============= 8. Fragment Shader ============= */
    /* Runs once per pixel the triangle covers and gives each one its color */
    const std::vector<char> fragShaderCode = ReadFile(fragFile);
    const VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    /* ============= 9. Depth Test ============= */
    /* Keeps a pixel only when it is nearer than the one already stored, then stores its depth */
    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    /* depthTestEnable controls whether a new pixel is compared with the stored depth */
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;
    /* depthWriteEnable controls whether a pixel that passes replaces the stored depth */
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
    /* depthCompareOp is the comparison. LESS keeps the pixel with the smaller depth, which is the nearer one */
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    /* depthBoundsTestEnable would also throw away pixels outside a chosen depth range */
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    /* The depth format has no stencil part, so there is nothing to test */
    depthStencilCreateInfo.stencilTestEnable = VK_FALSE;


    /* ============= 10. Color Blend ============= */
    /* The new color replaces the old one in the image. All four channels are written */
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    /* is a bitmask of VkColorComponentFlagBits specifying which of the R, G, B, and/or A components are enabled for writing */
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    /* ============= 11. Output Image ============= */
    /* The format of the image drawn into. This struct takes the place of a render pass */
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    /* is the number of entries in pColorAttachmentFormats.*/
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    /* is a pointer to an array of VkFormat values defining the format of color attachments used in this pipeline.*/
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
    /* is a VkFormat value defining the format of the depth attachment used in this pipeline. */
    pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

    /* ============= Dynamic State ============= */
    /* Not a pipeline step. It lists the settings left out of the pipeline, which FrameLoop sets each frame */
    const VkDynamicState dynamicStateEnables[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = 2;
    dynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;

    /* ============= Build The Pipeline ============= */
    /* Packs every struct above into one create call. The structs must stay alive until that call returns */
    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageInfo,
        fragShaderStageInfo
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
    /* is the description of binding locations used by both the pipeline and descriptor sets used with the pipeline.*/
    pipelineCreateInfo.layout = pipelineLayout;
    /*  the number of entries in the pStages array. */
    pipelineCreateInfo.stageCount = 2;
    /* a pointer to an array of stageCount VkPipelineShaderStageCreateInfo structures describing the set of the shader stages to be included in the graphics pipeline. */
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlending;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;

    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(gpuContext.GetDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    /* The pipeline keeps its own compiled copy of the shaders, so the modules can go */
    vkDestroyShaderModule(gpuContext.GetDevice(), vertShaderModule, nullptr);
    vkDestroyShaderModule(gpuContext.GetDevice(), fragShaderModule, nullptr);
}

void Pipeline::CreatePipelineLayout(const std::span<const VkDescriptorSetLayout> descriptorSetLayouts)
{
    /* The push constant: one model matrix, read by the vertex shader */
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ModelMatrixPushConstant);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    /* The descriptor sets. Entry i describes set i */
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    if (vkCreatePipelineLayout(gpuContext.GetDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}
