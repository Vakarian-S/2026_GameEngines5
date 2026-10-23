#pragma once
#include <string>
#include <array>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "GPUContext.h"
#include "Matrix.h"

/**
 * What differs between two pipelines, grouped by the section of CreateGraphicsPipeline that reads it.
 * Each field defaults to the mesh pipeline's value, so a filled copy names only what differs.
 * Fill it with designated initializers, in the order the fields are declared.
 */
struct PipelineSettings
{
    /** The most vertex fields a pipeline can read. Vertex uses 3. */
    static constexpr uint32_t kMaxAttributes = 4;
    /** The most sets a pipeline layout can hold. Vulkan guarantees at least 4 bound sets. */
    static constexpr uint32_t kMaxSetLayouts = 4;

    /** Step 1 - Vertex Input */
    struct VertexInput
    {
        VkVertexInputBindingDescription bindingDescription{};
        /** Only the first attributeDescriptionCount entries are used. */
        std::array<VkVertexInputAttributeDescription, kMaxAttributes> attributeDescriptions{};
        uint32_t attributeDescriptionCount = 0;
    } vertexInput;

    /** Step 2 - Input Assembly */
    struct InputAssembly
    {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    } inputAssembly;

    /** Step 3 - Vertex Shader */
    struct VertexShader
    {
        /** The compiled shader, as a path next to the executable. */
        std::string file;
    } vertexShader;
    
    /* Step 4 - Tesselation and Geometry */
    /* Step 5 - Viewport and Scissor */

    /** Step 6 - Rasterizer */
    struct Rasterizer
    {
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    } rasterizer;

    /** Step 8 - Fragment Shader */
    struct FragmentShader
    {
        /** The compiled shader, as a path next to the executable. */
        std::string file;
    } fragmentShader;

    /** Step 9 - Depth Test */
    struct DepthTest
    {
        /** Whether a pixel that passes replaces the stored depth. */
        bool write = true;
    } depthTest;

    /** Not a stage: the pipeline layout, which CreatePipelineLayout reads. */
    struct Layout
    {
        /** Entry i describes set i. Only the first setLayoutCount entries are used. */
        std::array<VkDescriptorSetLayout, kMaxSetLayouts> setLayouts{};
        uint32_t setLayoutCount = 0;
        /** The size of the vertex shader's push constant block. 0 means no push constant range. */
        uint32_t pushConstantSize = 0;
    } layout;
};

/**
 * The data pushed before each draw. The push constant block in the vertex shader must have the
 * same fields in the same order. Vulkan guarantees only 128 bytes for it.
 */
struct ModelMatrixPushConstant
{
    MATH::Matrix4 modelMatrix;
    /**
     * The normal matrix: the inverse transpose of the model matrix's upper 3x3. It keeps normals at
     * right angles to the surface under uneven scale. Each column is padded to 4 floats, because a
     * mat3 in the shader starts every column on a 16-byte boundary. w is unused.
     */
    MATH::Vec4 normalColumns[3];
};

/* 64 bytes of model matrix and 48 of padded normal matrix. 16 of the guaranteed 128 bytes stay free */
static_assert(sizeof(ModelMatrixPushConstant) == 112);

/**
 * The settings for every step that turns vertex data into pixels.
 * Built once at startup. FrameLoop binds it each frame to say "draw with these settings".
 */
class Pipeline
{
public:
    /**
     * Loads both shaders and builds the pipeline.
     * @param gpuContext_ the device to build on. It must outlive this object
     * @param colorFormat_ the format of the image the pipeline draws into, taken from the swapchain
     * @param depthFormat_ the format of the depth image the pipeline tests against, taken from the swapchain
     * @param settings_ settings for creating the pipeline, includes shaders and descriptorSetLayouts
     */
    explicit Pipeline(const GPUContext& gpuContext_, VkFormat colorFormat_, VkFormat depthFormat_,
                      const PipelineSettings& settings_);
    /** Destroys the pipeline, then the pipeline layout. */
    ~Pipeline();

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;

    /** @return the pipeline handle, which vkCmdBindPipeline takes */
    VkPipeline GetPipeline() const { return pipeline; }
    /** @return the pipeline layout handle, which push constants and descriptor sets take */
    VkPipelineLayout GetLayout() const { return pipelineLayout; }

private:
    /** Where the device comes from. */
    const GPUContext& gpuContext;

    /** The settings for every step of the graphics pipeline. It holds settings and never runs. */
    VkPipeline pipeline = VK_NULL_HANDLE;
    /** The list of variables the shaders expect from outside. */
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    /** The format of the image the pipeline draws into. It must match the swapchain's format. */
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    /** The format of the depth image the pipeline tests against. It must match the swapchain's depth format. */
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    /**
     * Reads a whole file into memory.
     * @param filename a path next to the executable, such as "shaders/triangle.vert.spv"
     * @return the bytes of the file
     */
    static std::vector<char> ReadFile(const std::string& filename);

    /**
     * Wraps compiled shader bytes in a Vulkan object, so a pipeline can be built from them.
     * @param code the bytes of one .spv file
     * @return the shader module. The caller destroys it once the pipeline is built
     */
    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;

    /**
     * Fills one settings struct per pipeline step, then builds the pipeline from all of them.
     * CreatePipelineLayout must run first, because the pipeline stores the layout.
     * @param settings Pipeline settings that contains the options to be used when creating the pipeline
     */
    void CreateGraphicsPipeline(const PipelineSettings& settings);

    /**
    * Creates the pipeline layout: the set layouts, and one push constant range for the vertex shader.
    * @param settings Pipeline settings that contains the descriptorSetLayouts
    */
    void CreatePipelineLayout(const PipelineSettings::Layout& settings);
};
