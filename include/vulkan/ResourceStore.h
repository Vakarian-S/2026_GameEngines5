#pragma once
#include <span>

#include "Buffer.h"
#include "GPUContext.h"
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
    * @param gpuContext_ the device to build on. It must outlive this object
    * @param temporaryCommandBuffers_ runs the copies into GPU memory. It must outlive this object
    */
    ResourceStore(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_);

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

private:
    /** Where the device comes from. */
    const GPUContext& gpuContext;

    /** Runs the copies into GPU memory when a mesh is created. */
    const TemporaryCommandBuffers& temporaryCommandBuffers;

    /** Every mesh, in creation order. A Mesh cannot move, because a Buffer cannot, so each one lives behind a pointer */
    std::vector<std::unique_ptr<Mesh>> meshList;
};
