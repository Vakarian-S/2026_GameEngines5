#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
#include "GPUContext.h"

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
     * @param vertFile_ the compiled vertex shader, as a path next to the executable
     * @param fragFile_ the compiled fragment shader, as a path next to the executable
     */
    explicit Pipeline(const GPUContext& gpuContext_, VkFormat colorFormat_, const std::string& vertFile_,
                      const std::string& fragFile_);
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
     */
    void CreateGraphicsPipeline(const std::string& vertFile, const std::string& fragFile);

    /** Creates the pipeline layout with no descriptor sets and no push constants. */
    void CreatePipelineLayout();
};
