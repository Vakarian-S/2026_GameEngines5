#pragma once
#include <vulkan/vulkan_core.h>
#include <array>
#include <string>

#include "GPUContext.h"
#include "TemporaryCommandBuffers.h"

class Buffer;

/**
 * A six-layer image, loaded from 6 files in a folder. It owns the image, the memory behind it,
 * the view the shader reads through, and the sampler that says how the shader reads it.
 */
class CubeMap
{
public:
    /**
     * Loads 6 images from one folder and copies their pixels into GPU memory, through a staging buffer.
     * @param gpuContext_ the device to build on. It must outlive this object
     * @param temporaryCommandBuffers_ runs the copy into the image. Only used inside this constructor
     * @param folder_ the images folder path, as a path next to the executable, such as "assets/skyboxes/space"
     */
    CubeMap(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_,
            const std::string& folder_);
    ~CubeMap();

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    CubeMap(const CubeMap&) = delete;
    CubeMap& operator=(const CubeMap&) = delete;
    CubeMap(CubeMap&&) = delete;
    CubeMap& operator=(CubeMap&&) = delete;

    /** @return the view the shader reads the image through, which a descriptor set points at */
    VkImageView GetImageView() const { return imageView; }
    /** @return the sampler, which a descriptor set points at next to the view */
    VkSampler GetSampler() const { return sampler; }

private:
    /**
     * Four bytes per pixel: red, green, blue, alpha. SRGB because image files store sRGB colour.
     * The GPU turns it into linear values when the shader samples it.
     */
    static constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_SRGB;

    /** Names of all the faces of the cubemap in order +X -X +Y -Y +Z -Z */
    static constexpr std::array<const char*, 6> kFaceNames{"px", "nx", "py", "ny", "pz", "nz"};

    /** Where the device comes from. */
    const GPUContext& gpuContext;

    /** The image itself. It has no memory until imageMemory is bound to it. */
    VkImage image = VK_NULL_HANDLE;
    /** The memory behind the image, in GPU-only memory. */
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    /** How the shader looks at the image: as 6 2d Layers with one mip level. */
    VkImageView imageView = VK_NULL_HANDLE;
    /** How the shader reads between pixels and past the edges. It holds settings, not data. */
    VkSampler sampler = VK_NULL_HANDLE;
    /** Size in pixels of each face */
    uint32_t faceSize = 0;

    /** Creates the image and its memory. the face size must be set first */
    void CreateImage();

    /**
    * Copies the staging buffer into the image and leaves the image ready for the shader.
    * @param temporaryCommandBuffers runs the copy and waits for it
    * @param stagingBuffer holds the pixels, in memory the CPU wrote
    */
    void UploadPixels(const TemporaryCommandBuffers& temporaryCommandBuffers, const Buffer& stagingBuffer) const;

    /** Creates the view the shader reads the image through. */
    void CreateImageView();

    /** Creates the sampler: linear filtering, clamp at the edges, the GPU's highest anisotropy. */
    void CreateSampler();
};
