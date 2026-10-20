#include "vulkan/FrameLoop.h"
#include "vulkan/Buffer.h"
#include "vulkan/Pipeline.h"

#include <cassert>
#include <cstdint>
#include <span>
#include <stdexcept>

#include "utils/Profiler.h"
#include "vulkan/ImageBarrier.h"


FrameLoop::FrameLoop(const GPUContext& gpuContext_, Swapchain& swapchain_, const Descriptors& descriptors_) :
    gpuContext(gpuContext_),
    swapchain(swapchain_),
    descriptors(descriptors_)
{
    CreateCommandPool();
    CreateCommandBuffers();
    CreateSyncObjects();
}

FrameLoop::~FrameLoop()
{
    const VkDevice device = gpuContext.GetDevice();
    /* The GPU may still be running the last frames, and an object in use must not be destroyed */
    vkDeviceWaitIdle(device);

    /* Each vector has its own length, so each gets its own loop */
    DestroyRenderFinishedSemaphores();
    for (const VkSemaphore semaphore : imageAvailableSemaphores) vkDestroySemaphore(device, semaphore, nullptr);
    for (const VkFence fence : inFlightFences) vkDestroyFence(device, fence, nullptr);

    /* Destroying the pool frees the command buffers allocated from it */
    vkDestroyCommandPool(device, commandPool, nullptr);
}


void FrameLoop::DrawFrame(const VkClearColorValue& clearColor, const Pipeline& pipeline,
                          const ResourceStore& resourceStore, const DrawList& drawList, const CameraUniform& camera,
                          const LightUniform& lights)
{
    /**
    At a high level, rendering a frame in Vulkan consists of a common set of steps:
    - Wait for the previous frame to finish
    - Acquire an image from the swap chain
    - Record a command buffer which draws the scene onto that image
    - Submit the recorded command buffer
    - Present the swap chain image
    **/

    /* ============= 1. Wait For This Slot ============= */
    /* Rebuilds a stale swapchain, then waits until the GPU has finished this slot's last frame */

    /* A stale swapchain is rebuilt before anything else. While minimized, the frame is skipped */
    if (swapchainNeedsRebuild && !RebuildSwapchain()) return;

    if (vkWaitForFences(gpuContext.GetDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to wait for fence!");
    }

    /* ============= 2. Write The Descriptors ============= */
    /* The fence above proves the GPU finished this slot's last frame, so its buffers are free to overwrite */
    descriptors.WriteCamera(currentFrame, camera);
    descriptors.WriteLights(currentFrame, lights);

    /* ============= 3. Acquire An Image ============= */
    /* Asks the swapchain for an image to draw into, and handles a swapchain that no longer fits the window */

    /* Index of the image currently available in our swapchain */
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(gpuContext.GetDevice(), swapchain.GetSwapchain(), UINT64_MAX,
                                            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    /* The swapchain no longer fits the window. No image was handed out, so nothing is drawn */
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        swapchainNeedsRebuild = true;
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    /* Suboptimal still works, so this frame is drawn and the rebuild waits for the next one */
    if (result == VK_SUBOPTIMAL_KHR) swapchainNeedsRebuild = true;

    /* ============= 4. Record ============= */
    /* Clears this slot's fence and command buffer, then records this frame's commands into it */

    /* Reset only now. A reset above the early return would leave the fence unsignalled forever */
    vkResetFences(gpuContext.GetDevice(), 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    {
        /* Times the CPU side of recording: one push and one draw per item. The GPU's work is not in here */
        PROFILE_SCOPE("Record");
        RecordCommandBuffer(imageIndex, clearColor, pipeline, resourceStore, drawList);
    }

    /* ============= 5. Submit ============= */
    /* Hands the commands to the graphics queue. The GPU waits for the image, then signals when drawing is done */
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const VkSemaphore imageAvailable[] = {imageAvailableSemaphores[currentFrame]};
    constexpr VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    /* waitSemaphoreCount is the number of semaphores upon which to wait before executing the command buffers for the batch. */
    submitInfo.waitSemaphoreCount = 1;
    /* is a pointer to an array of VkSemaphore handles upon which to wait before the command buffers for this batch begin execution. */
    submitInfo.pWaitSemaphores = imageAvailable;
    /* pWaitDstStageMask is a pointer to an array of pipeline stages at which each corresponding semaphore wait will occur. */
    submitInfo.pWaitDstStageMask = waitStages;

    /* commandBufferCount is the number of command buffers to execute in the batch */
    submitInfo.commandBufferCount = 1;
    /* pCommandBuffers is a pointer to an array of VkCommandBuffer handles to execute in the batch. */
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    const VkSemaphore renderFinished[] = {renderFinishedSemaphores[imageIndex]};
    /* signalSemaphoreCount is the number of semaphores to be signaled once the commands specified in pCommandBuffers have completed execution. */
    submitInfo.signalSemaphoreCount = 1;
    /* pSignalSemaphores is a pointer to an array of VkSemaphore handles which will be signaled when the command buffers for this batch have completed execution */
    submitInfo.pSignalSemaphores = renderFinished;


    if (vkQueueSubmit(gpuContext.GetGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    /* ============= 6. Present ============= */
    /* Asks the present queue to show the image once drawing is done, then moves on to the next frame slot */
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    /* waitSemaphoreCount is the number of semaphores to wait for before issuing the present request. The number may be zero. */
    presentInfo.waitSemaphoreCount = 1;
    /* WaitSemaphores is NULL or a pointer to an array of VkSemaphore objects with waitSemaphoreCount entries,
     * and specifies the semaphores to wait for before issuing the present request.*/
    presentInfo.pWaitSemaphores = renderFinished;

    const VkSwapchainKHR swapChains[] = {swapchain.GetSwapchain()};
    /* swapchainCount is the number of swapchains being presented to by this command. */
    presentInfo.swapchainCount = 1;
    /* pSwapchains is a pointer to an array of VkSwapchainKHR objects with swapchainCount entries.*/
    presentInfo.pSwapchains = swapChains;
    /* pImageIndices is a pointer to an array of indices into the array of each swapchain’s presentable images, with swapchainCount entries.*/
    presentInfo.pImageIndices = &imageIndex;
    result = vkQueuePresentKHR(gpuContext.GetPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        swapchainNeedsRebuild = true;
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
}


void FrameLoop::CreateCommandPool()
{
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = gpuContext.GetQueueFamilyIndices().graphicsFamily.value();
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(gpuContext.GetDevice(), &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool!");
    }
}

void FrameLoop::CreateCommandBuffers()
{
    commandBuffers.resize(kMaxFramesInFlight);
    VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.commandPool = commandPool;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocInfo.commandBufferCount = kMaxFramesInFlight;

    if (vkAllocateCommandBuffers(gpuContext.GetDevice(), &commandBufferAllocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void FrameLoop::CreateSyncObjects()
{
    assert(imageAvailableSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

    imageAvailableSemaphores.resize(kMaxFramesInFlight);
    inFlightFences.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    CreateRenderFinishedSemaphores();
    for (size_t i = 0; i < kMaxFramesInFlight; i++)
    {
        if (vkCreateSemaphore(gpuContext.GetDevice(), &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[i]) !=
            VK_SUCCESS ||
            vkCreateFence(gpuContext.GetDevice(), &fenceCreateInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS
        )
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void FrameLoop::RecordCommandBuffer(const uint32_t imageIndex, const VkClearColorValue& clearColor,
                                    const Pipeline& pipeline, const ResourceStore& resourceStore,
                                    const DrawList& drawList) const
{
    /* ============= 1. Begin Recording ============= */
    /* Opens this slot's command buffer. Every vkCmd call below adds to it */
    const VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    /* ============= 2. Prepare The Images ============= */
    /* Moves the colour image and the depth image into the layouts that drawing needs */

    /*
     * The swapchain image becomes something to draw into. Its old contents are thrown away, so
     * there is no earlier work to wait for
     */

    RecordImageBarrier(
        commandBuffer,
        swapchain.GetImages()[imageIndex],
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        {
            .srcAccessMask = {},
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        }
    );
    /*
     * The depth image is cleared every frame, so its old contents may be thrown away. The masks wait
     * for the previous frame's depth tests, because both frames share this one image
     */
    RecordImageBarrier(
        commandBuffer,
        swapchain.GetDepthImage(),
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        {
            .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
        }
    );

    /* ============= 3. Begin Rendering ============= */
    /* Names the two images drawn into, clears both, and starts drawing */

    /* The colour attachment. It is cleared to clearColor and kept afterwards, because the screen shows it */
    VkRenderingAttachmentInfo attachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        /* imageView is the image view that will be used for rendering.*/
        .imageView = swapchain.GetImageViews()[static_cast<int>(imageIndex)],
        /* imageLayout is the layout that imageView will be in during rendering. */
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        /* Defines the load operation for the attachment. */
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        /* Defines the store operation for the attachment */
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = clearColor},
    };

    /* The depth attachment. Clearing to 1.0 means "as far as possible", so every real pixel is nearer */
    const VkRenderingAttachmentInfo depthAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = swapchain.GetDepthImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        /* Nothing reads the depth after the frame, so the GPU need not keep it */
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {.depthStencil = {.depth = 1.0f, .stencil = 0}},
    };
    const VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        /* renderArea is the render area that is affected by the render pass instance */
        .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapchain.GetExtent()},
        /* layerCount is the number of layers rendered to in each attachment when viewMask is 0. */
        .layerCount = 1,
        /* colorAttachmentCount is the number of elements in pColorAttachments. */
        .colorAttachmentCount = 1,
        /* pColorAttachments is a pointer to an array of colorAttachmentCount VkRenderingAttachmentInfo structures describing any color attachments used. */
        .pColorAttachments = &attachmentInfo,
        /* pDepthAttachment is a pointer to a VkRenderingAttachmentInfo structure describing a depth attachment. */
        .pDepthAttachment = &depthAttachmentInfo
    };
    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    /* ============= 4. Bind State ============= */
    /* Sets everything the draws share: pipeline, camera set, viewport, scissor, vertices and indices */

    /* From here on, every draw in this command buffer uses the pipeline's settings */
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());

    /* Points the shader's set 0 at this slot's camera buffer. The draws that follow read their camera there */
    const VkDescriptorSet descriptorSet = descriptors.GetSet(currentFrame);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(), 0, 1,
                            &descriptorSet, 0, nullptr);
    /* The viewport stretches the picture over the whole swapchain image */
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain.GetExtent().width);
    viewport.height = static_cast<float>(swapchain.GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    /* The scissor trims every pixel outside its rectangle. Here it covers the whole image */
    const VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = swapchain.GetExtent()};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    /* ============= 5. Draw ============= */
    /* One push and one draw per item. A mesh's buffers and a material's set are bound again only when they change */
    /* No mesh and no material are bound yet. No handle has this index, so the first item always binds both */
    uint32_t boundMesh = UINT32_MAX;
    uint32_t boundMaterial = UINT32_MAX;
    for (const DrawItem& item : drawList)
    {
        const Mesh& mesh = resourceStore.GetMesh(item.mesh);
        if (item.mesh.index != boundMesh)
        {
            /* The draws that follow read their vertices from this buffer, starting at byte 0 */
            const VkBuffer vertexBuffers[] = {mesh.vertexBuffer.GetBuffer()};
            constexpr VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            /* The draws that follow read their corner numbers from this buffer, as 32-bit values */
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            boundMesh = item.mesh.index;
        }
        if (item.material.index != boundMaterial)
        {
            /* Points the shader's set 1 at this material. Set 0, the camera and the lights, stays bound */
            const VkDescriptorSet materialSet = resourceStore.GetMaterialSet(item.material);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetLayout(), 1, 1,
                                    &materialSet, 0, nullptr);
            boundMaterial = item.material.index;
        }
        const ModelMatrixPushConstant pushConstant{.modelMatrix = item.transform};
        vkCmdPushConstants(commandBuffer, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(ModelMatrixPushConstant), &pushConstant);
        vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
    }

    /* ============= 6. Finish ============= */
    /* Ends drawing, readies the colour image for the screen, and closes the command buffer */
    vkCmdEndRendering(commandBuffer);

    /* The finished colour image becomes something the screen can show. Nothing after this frame waits on it */
    RecordImageBarrier(
        commandBuffer,
        swapchain.GetImages()[imageIndex],
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        {
            .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = {},
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
        }
    );
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to end the command buffer!");
    }
}

void FrameLoop::CreateRenderFinishedSemaphores()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    renderFinishedSemaphores.resize(swapchain.GetImages().size());
    for (VkSemaphore& semaphore : renderFinishedSemaphores)
    {
        if (vkCreateSemaphore(gpuContext.GetDevice(), &semaphoreCreateInfo, nullptr, &semaphore) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create a render finished semaphore!");
        }
    }
}

void FrameLoop::DestroyRenderFinishedSemaphores()
{
    for (const VkSemaphore semaphore : renderFinishedSemaphores)
    {
        vkDestroySemaphore(gpuContext.GetDevice(), semaphore, nullptr);
    }
    renderFinishedSemaphores.clear();
}

bool FrameLoop::RebuildSwapchain()
{
    if (!swapchain.Recreate()) return false;

    /* These belong to the images, and the new swapchain may hold a different number of them */
    DestroyRenderFinishedSemaphores();
    CreateRenderFinishedSemaphores();

    swapchainNeedsRebuild = false;
    return true;
}
