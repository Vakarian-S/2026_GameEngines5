#pragma once
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

#include "Buffer.h"
#include "GPUContext.h"
#include "Texture.h"
#include "MaterialDescriptors.h"
#include "Vertex.h"
#include "core/Material.h"
#include "core/ResourceLoader.h"
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

/** One material in GPU memory: the buffer that holds its surface values, and the set that points at that buffer and at its texture. */
struct Material
{
    /**
     * Copies the surface values into GPU memory, then allocates the set that points at them.
     * @param gpuContext the device to build on. It must outlive this material
     * @param temporaryCommandBuffers runs the copy into GPU memory
     * @param materialDescriptors allocates the set
     * @param albedo the texture the set points at. It must outlive this material
     * @param values the surface values. They are copied once and never change
     */
    Material(const GPUContext& gpuContext, const TemporaryCommandBuffers& temporaryCommandBuffers,
             const MaterialDescriptors& materialDescriptors, const Texture& albedo, const MaterialUniform& values);

    /** Declared before descriptorSet, because the set points at it and so it must exist first. */
    Buffer uniformBuffer;
    /** The set FrameLoop binds as set 1. The pool frees it */
    VkDescriptorSet descriptorSet;
};

/**
 * Owns every mesh, texture and material the renderer can draw. A handle is a position in its list,
 * so many draw items can share one mesh or one material.
 */
class ResourceStore : public ResourceLoader
{
public:
    /**
    * @param gpuContext_ The device to build on. It must outlive this object
    * @param temporaryCommandBuffers_ Runs the copies into GPU memory. It must outlive this object
    * @param materialDescriptors_ Allocates one set per material. It must outlive this object
    */
    ResourceStore(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_,
                  const MaterialDescriptors& materialDescriptors_);

    /* Holds Vulkan handles that the destructor destroys, so no copies or moves */
    ResourceStore(const ResourceStore&) = delete;
    ResourceStore& operator=(const ResourceStore&) = delete;
    ResourceStore(ResourceStore&&) = delete;
    ResourceStore& operator=(ResourceStore&&) = delete;


    /**
     * Copies a unit cube into GPU memory.
     * @return the handle that names the cube in draw items
     */
    MeshHandle CreateCube() override;

    /**
    * Reads an OBJ file and copies its mesh into GPU memory.
    * @param path the OBJ file, as a path next to the executable, such as "assets/meshes/Mario.obj"
    * @return the handle that names the mesh in draw items
    */
    MeshHandle CreateMesh(const std::string& path) override;

    /**
    * Loads an image file into GPU memory.
    * @param path the image file, as a path next to the executable, such as "assets/textures/skull_texture.jpg"
    * @return the handle that names the texture in a MaterialDesc
    */
    TextureHandle CreateTexture(const std::string& path) override;

    /**
     * Copies a material's surface values into GPU memory and allocates the set that points at them and at its texture.
     * @param desc the texture and the surface values. The texture must come from this store
     * @return the handle that names the material in draw items
     */
    MaterialHandle CreateMaterial(const MaterialDesc& desc) override;

    /**
     * @param material a handle this store returned
     * @return the set that points at that material, which FrameLoop binds as set 1
     */
    VkDescriptorSet GetMaterialSet(MaterialHandle material) const;

    /**
     * @param mesh a handle this store returned
     * @return the mesh it names
     */
    const Mesh& GetMesh(MeshHandle mesh) const;

private:
    /** Where the device comes from. */
    const GPUContext& gpuContext;

    /** Runs the copies into GPU memory when a mesh, a texture or a material is created. */
    const TemporaryCommandBuffers& temporaryCommandBuffers;

    /** Allocates one set per material when a material is created. */
    const MaterialDescriptors& materialDescriptors;

    /** Every mesh, in creation order. A Mesh cannot move, because a Buffer cannot, so each one lives behind a pointer */
    std::vector<std::unique_ptr<Mesh>> meshList;

    /** Every texture, in creation order. A Texture cannot move, so each one lives behind a pointer */
    std::vector<std::unique_ptr<Texture>> textureList;

    /** Every material, in creation order. Declared after textureList, so C++ destroys the materials before the textures they point at */
    std::vector<std::unique_ptr<Material>> materialList;
    
    /** Cache for the cube, we only have one so we store that handle */
    std::optional<MeshHandle> cubeCache;
    /** Cache for the mesh, we search by path */
    std::unordered_map<std::string, MeshHandle> meshCache;
    /** Cache for the texture, we search by path */
    std::unordered_map<std::string, TextureHandle> textureCache;
    /** Cache for the materials, we search by values */
    std::vector<std::pair<MaterialDesc, MaterialHandle>> materialCache;
};
