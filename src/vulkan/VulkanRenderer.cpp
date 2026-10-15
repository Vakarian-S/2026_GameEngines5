#include "vulkan/VulkanRenderer.h"
#include "core/Camera.h"
#include <array>
#include "MMath.h"


VulkanRenderer::VulkanRenderer(SDL_Window* sdlWindow_) : gpuContext(sdlWindow_),
                                                         descriptors(gpuContext),
                                                         temporaryCommandBuffers(gpuContext),
                                                         swapchain(gpuContext, sdlWindow_),
                                                         /* Entry i describes set i of the shaders. Today there is only set 0, the camera */
                                                         pipeline(gpuContext, swapchain.GetImageFormat(),
                                                                  swapchain.GetDepthFormat(),
                                                                  "shaders/mesh.vert.spv",
                                                                  "shaders/mesh.frag.spv",
                                                                  std::array{descriptors.GetLayout()}),
                                                         resourceStore(gpuContext, temporaryCommandBuffers),
                                                         frameLoop(gpuContext, swapchain, descriptors)
{
}

void VulkanRenderer::DrawFrame(const Camera& camera, const DrawList& drawList)
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

    frameLoop.DrawFrame(kClearColor, pipeline, resourceStore, drawList, cameraUniform);
}
