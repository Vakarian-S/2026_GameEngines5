#include "vulkan/ResourceStore.h"

#include <cassert>

#include "Vector.h"

Mesh::Mesh(const GPUContext& gpuContext, const TemporaryCommandBuffers& temporaryCommandBuffers,
           const std::span<const Vertex> vertices, const std::span<const uint32_t> indices) :
    vertexBuffer(gpuContext, temporaryCommandBuffers, vertices.data(), vertices.size_bytes(),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
    indexBuffer(gpuContext, temporaryCommandBuffers, indices.data(), indices.size_bytes(),
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
    indexCount(static_cast<uint32_t>(indices.size()))
{
}

ResourceStore::ResourceStore(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_) :
    gpuContext(gpuContext_),
    temporaryCommandBuffers(temporaryCommandBuffers_)
{
}

MeshHandle ResourceStore::CreateCube()
{
    /* The eight corners of a cube: a position and a colour each */
    constexpr Vertex cubeVertices[] = {
        {.position = MATH::Vec3(-0.5f, -0.5f, 0.5f), .color = MATH::Vec3(1.0, 0.37f, 0.38f)},
        {.position = MATH::Vec3(0.5f, -0.5f, 0.5f), .color = MATH::Vec3(1.0f, 0.6f, 0.4f)},
        {.position = MATH::Vec3(0.5f, 0.5f, 0.5f), .color = MATH::Vec3(0.5f, 0.0, 0.5)},
        {.position = MATH::Vec3(-0.5f, 0.5f, 0.5f), .color = MATH::Vec3(0.18f, 0.11f, 0.41f)},
        {.position = MATH::Vec3(-0.5f, -0.5f, -0.5f), .color = MATH::Vec3(1.0, 0.37f, 0.38f)},
        {.position = MATH::Vec3(0.5f, -0.5f, -0.5f), .color = MATH::Vec3(1.0f, 0.6f, 0.4f)},
        {.position = MATH::Vec3(0.5f, 0.5f, -0.5f), .color = MATH::Vec3(0.5f, 0.0, 0.5)},
        {.position = MATH::Vec3(-0.5f, 0.5f, -0.5f), .color = MATH::Vec3(0.18f, 0.11f, 0.41f)},
    };
    /* Each number is a position in cubeVertices. Every three numbers make one triangle */
    constexpr uint32_t cubeIndices[] = {
        /* near face */
        4, 5, 6, 6, 7, 4,
        /* far face */
        0, 2, 1, 0, 3, 2,
        /* right face */
        5, 1, 2, 2, 6, 5,
        /* left face */
        0, 4, 7, 7, 3, 0,
        /* top face */
        0, 1, 5, 5, 4, 0,
        /* bottom face */
        3, 7, 6, 6, 2, 3,
    };
    /* The new mesh's handle is its position in the list, so read the size before adding it */
    const MeshHandle handle{static_cast<uint32_t>(meshList.size())};
    /* Both arrays are copied into GPU memory once, here. The frame loop only reads them */
    meshList.push_back(std::make_unique<Mesh>(gpuContext, temporaryCommandBuffers, cubeVertices, cubeIndices));
    return handle;
}

const Mesh& ResourceStore::GetMesh(const MeshHandle mesh) const
{
    assert(mesh.index < meshList.size());
    return *meshList[mesh.index];
}
