#pragma once
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "Buffer.h"
#include "GPUContext.h"
#include "Texture.h"
#include "TextureDescriptors.h"
#include "Vertex.h"
#include "core/Scene.h"

class TemporaryCommandBuffers;

/** One mesh in GPU memory: its corners, the corner numbers that make its triangles, and how many numbers there are. */
struct Mesh
{
    /**
     * Copies both arrays into GPU memory.
     * @param gpuContext the device to build on. It must outlive this mesh
     * @param temporaryCommandBuffers runs the copies into GPU memory
     * @param vertices the corners of the mesh
     * @param indices the corner numbers. Every three make one triangle
     */
    Mesh(const GPUContext& gpuContext, const TemporaryCommandBuffers& temporaryCommandBuffers,
         std::span<const Vertex> vertices, std::span<const uint32_t> indices);

    Buffer vertexBuffer;
    Buffer indexBuffer;
    uint32_t indexCount;
};

/**
 * Owns every mesh the renderer can draw. A MeshHandle is a position in its list, so many draw items
 * can share one mesh.
 */
class ResourceStore
{
public:
    /**
    * @param gpuContext_ The device to build on. It must outlive this object
    * @param temporaryCommandBuffers_ Runs the copies into GPU memory. It must outlive this object
    * @param textureDescriptors_ Allocates one set per texture. It must outlive this object
    */
    ResourceStore(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_,
                  const TextureDescriptors& textureDescriptors_);

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    ResourceStore(const ResourceStore&) = delete;
    ResourceStore& operator=(const ResourceStore&) = delete;
    ResourceStore(ResourceStore&&) = delete;
    ResourceStore& operator=(ResourceStore&&) = delete;


    /**
     * Copies a unit cube into GPU memory.
     * @return the handle that names the cube in draw items
     */
    MeshHandle CreateCube();

    /**
     * @param mesh a handle this store returned
     * @return the mesh it names
     */
    const Mesh& GetMesh(MeshHandle mesh) const;

    /**
    * Loads an image file into GPU memory and allocates the set that points at it.
    * @param path the image file, as a path next to the executable, such as "assets/textures/skull_texture.jpg"
    * @return the handle that names the texture in draw items
    */
    TextureHandle CreateTexture(const std::string& path);

    /**
     * @param texture a handle this store returned
     * @return the set that points at that texture, which FrameLoop binds as set 1
     */
    VkDescriptorSet GetTextureSet(TextureHandle texture) const;

private:
    /** Where the device comes from. */
    const GPUContext& gpuContext;

    /** Runs the copies into GPU memory when a mesh is created. */
    const TemporaryCommandBuffers& temporaryCommandBuffers;
    
    /** Allocates one set per texture when a texture is created. */
    const TextureDescriptors& textureDescriptors;

    /** Every mesh, in creation order. A Mesh cannot move, because a Buffer cannot, so each one lives behind a pointer */
    std::vector<std::unique_ptr<Mesh>> meshList;

    /** Every texture, in creation order. A Texture cannot move, so each one lives behind a pointer */
    std::vector<std::unique_ptr<Texture>> textureList;

    /** One set per texture: textureSets[i] points at textureList[i]. The pool owns them */
    std::vector<VkDescriptorSet> textureSets;
};
