#include "vulkan/VulkanRenderer.h"
#include "core/Camera.h"
#include "MMath.h"

#include <stdexcept>
#include <string>

#include "core/PresentMode.h"
#include "vulkan/PipelineSettings.h"

namespace
{
    /**
     * Maps the present mode enum into actual Vulkan Presents modes
     * @param presentMode present mode to lookup 
     * @return Vulkan Type present mode
     */
    VkPresentModeKHR PresentModeToVulkan(const PresentMode presentMode)
    {
        switch (presentMode)
        {
        case PresentMode::Fifo: return VK_PRESENT_MODE_FIFO_KHR;
        case PresentMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        case PresentMode::Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR;
        case PresentMode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case PresentMode::Count: return VK_PRESENT_MODE_FIFO_KHR;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /**
     * Maps the given Vulkan Present mode in to the abstract Present mode enum
     * @param presentMode Vulkan Present mode to lookup
     * @return PresentMode Enum value
     */
    PresentMode PresentModeFromVulkan(const VkPresentModeKHR presentMode)
    {
        switch (presentMode)
        {
        case VK_PRESENT_MODE_FIFO_KHR: return PresentMode::Fifo;
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return PresentMode::FifoRelaxed;
        case VK_PRESENT_MODE_MAILBOX_KHR: return PresentMode::Mailbox;
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return PresentMode::Immediate;
        case PresentMode::Count: return PresentMode::Fifo;
        /* Non supported */
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
        case VK_PRESENT_MODE_FIFO_LATEST_READY_KHR:
        case VK_PRESENT_MODE_MAX_ENUM_KHR:
            return PresentMode::Fifo;
        }
        return PresentMode::Fifo;
    }
}

VulkanRenderer::VulkanRenderer(SDL_Window* sdlWindow_, const PresentMode startPresentMode) :
    gpuContext(sdlWindow_),
    descriptors(gpuContext),
    materialDescriptors(gpuContext),
    temporaryCommandBuffers(gpuContext),
    swapchain(gpuContext, sdlWindow_, PresentModeToVulkan(startPresentMode)),
    resourceStore(gpuContext, temporaryCommandBuffers,
                  materialDescriptors),
    meshPipeline(gpuContext, swapchain.GetImageFormat(),
                 swapchain.GetDepthFormat(),
                 MeshPipelineSettings(
                     descriptors.GetLayout(),
                     materialDescriptors.GetLayout())),
    linePipeline(gpuContext, swapchain.GetImageFormat(),
                 swapchain.GetDepthFormat(), LinePipelineSettings(
                     descriptors.GetLayout())),
    lineBuffers(gpuContext),
    frameLoop(gpuContext, swapchain, descriptors, lineBuffers)
{
}

void VulkanRenderer::DrawFrame(const Camera& camera, const SceneLighting& lighting, const DrawList& drawList,
                               const DebugLines& debugLines)
{
    /* Width over height, read every frame, so a resize keeps the cubes' proportions */
    const auto [width, height] = swapchain.GetExtent();
    const float aspect = static_cast<float>(width) / static_cast<float>(height);

    /* The scene chose the view and the lens. The Vulkan-only parts of the projection are added here */
    CameraUniform cameraUniform{
        .view = camera.view,
        .projection = MMath::PerspectiveVulkan(camera.fovYDegrees, aspect, camera.nearPlane, camera.farPlane)
    };

    /* Element 5 is the Y scale. The math library assumes +Y is up, and Vulkan's +Y points down the screen */
    cameraUniform.projection[5] *= -1.0f;

    /* The view matrix moves the world so the camera sits at the origin. Its inverse moves the origin back to the camera, so its last column is the camera's position */
    const MATH::Matrix4 cameraToWorld = MMath::inverse(camera.view);
    cameraUniform.position = MATH::Vec4(cameraToWorld[12], cameraToWorld[13], cameraToWorld[14], 1.0f);

    /* The uniform buffer has room for kMaxLights. Saying so here names the count, where the shader would drop the extra lights */
    if (lighting.lights.size() > kMaxLights)
    {
        throw std::runtime_error("the scene has " + std::to_string(lighting.lights.size()) +
            " lights, and the renderer takes at most " + std::to_string(kMaxLights));
    }

    LightUniform lightUniform{};
    for (std::size_t i = 0; i < lighting.lights.size(); ++i)
    {
        const Light& light = lighting.lights[i];
        lightUniform.lights[i].position = MATH::Vec4(light.position.x, light.position.y, light.position.z, 1.0f);
        lightUniform.lights[i].color = MATH::Vec4(light.color.x, light.color.y, light.color.z, 1.0f);
    }
    lightUniform.ambient = MATH::Vec4(lighting.ambient.x, lighting.ambient.y, lighting.ambient.z, 1.0f);
    lightUniform.count = static_cast<uint32_t>(lighting.lights.size());

    /* One reference per pipeline. FrameLoop records one pass with each */
    const FramePipelines pipelines{.meshPipeline = meshPipeline, .linePipeline = linePipeline};
    frameLoop.DrawFrame(kClearColor, pipelines, resourceStore, drawList, debugLines, cameraUniform, lightUniform);
}

PresentMode VulkanRenderer::GetPresentMode() const
{
    return PresentModeFromVulkan(swapchain.GetUsedPresentMode());
}

void VulkanRenderer::SetPresentMode(const PresentMode presentMode)
{
    swapchain.SetWantedPresentMode(PresentModeToVulkan(presentMode));
    frameLoop.RequestSwapchainRebuild();
}

bool VulkanRenderer::SupportsPresentMode(const PresentMode mode) const
{
    const std::vector<VkPresentModeKHR>& modes = swapchain.GetSupportedPresentModes();
    return std::ranges::find(modes, PresentModeToVulkan(mode)) != modes.end();
}

void VulkanRenderer::RequestSwapchainRebuild()
{
    frameLoop.RequestSwapchainRebuild();
}
