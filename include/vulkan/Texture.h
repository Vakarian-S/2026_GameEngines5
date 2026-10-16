#pragma once
#include "GPUContext.h"
#include "TemporaryCommandBuffers.h"

class Buffer;
/**
 * One image the shaders can sample, loaded from a file. It owns the image, the memory behind it,
 * the view the shader reads through, and the sampler that says how the shader reads it.
 */
class Texture
{
public:
    /**
     * Loads an image file and copies its pixels into GPU memory, through a staging buffer.
     * @param gpuContext_ the device to build on. It must outlive this object
     * @param temporaryCommandBuffers_ runs the copy into the image. Only used inside this constructor
     * @param path_ the image file, as a path next to the executable, such as "assets/textures/skull_texture.jpg"
     */
    Texture(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_,
            const std::string& path_);

    /** Destroys the sampler, the view, and the image, then frees the memory. */
    ~Texture();

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;

    /** @return the view the shader reads the image through, which a descriptor set points at */
    VkImageView GetImageView() const { return imageView; }
    /** @return the sampler, which a descriptor set points at next to the view */
    VkSampler GetSampler() const { return sampler; }
    /** @return the width of the image in pixels */
    uint32_t GetWidth() const { return width; }
    /** @return the height of the image in pixels */
    uint32_t GetHeight() const { return height; }

private:
    /**
    * Four bytes per pixel: red, green, blue, alpha. SRGB because image files store sRGB colour.
    * The GPU turns it into linear values when the shader samples it.
    */
    static constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_SRGB;

    /** Where the device comes from. */
    const GPUContext& gpuContext;

    /** The image itself. It has no memory until imageMemory is bound to it. */
    VkImage image = VK_NULL_HANDLE;
    /** The memory behind the image, in GPU-only memory. */
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    /** How the shader looks at the image: a flat 2D colour image with one mip level. */
    VkImageView imageView = VK_NULL_HANDLE;
    /** How the shader reads between pixels and past the edges. It holds settings, not data. */
    VkSampler sampler = VK_NULL_HANDLE;

    /** The width of the image in pixels. */
    uint32_t width = 0;
    /** The height of the image in pixels. */
    uint32_t height = 0;

    /** Creates the image and its memory. width and height must be set first. */
    void CreateImage();

    /**
    * Copies the staging buffer into the image and leaves the image ready for the shader.
    * @param temporaryCommandBuffers runs the copy and waits for it
    * @param stagingBuffer holds the pixels, in memory the CPU wrote
    */
    void UploadPixels(const TemporaryCommandBuffers& temporaryCommandBuffers, const Buffer& stagingBuffer) const;

    /** Creates the view the shader reads the image through. */
    void CreateImageView();

    /** Creates the sampler: linear filtering, repeat at the edges, the GPU's highest anisotropy. */
    void CreateSampler();
};
