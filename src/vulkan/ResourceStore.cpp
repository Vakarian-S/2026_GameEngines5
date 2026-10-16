#include "vulkan/ResourceStore.h"

#include <cassert>
#include <stdexcept>
#include <string>

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

ResourceStore::ResourceStore(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_,
                             const TextureDescriptors& textureDescriptors_) :
    gpuContext(gpuContext_),
    temporaryCommandBuffers(temporaryCommandBuffers_),
    textureDescriptors(textureDescriptors_)

{
}

MeshHandle ResourceStore::CreateCube()
{
    /* Shared by all four corners that had it before, so the faces keep their old tint */
    constexpr MATH::Vec3 coral(1.0f, 0.37f, 0.38f);
    constexpr MATH::Vec3 peach(1.0f, 0.6f, 0.4f);
    constexpr MATH::Vec3 purple(0.5f, 0.0f, 0.5f);
    constexpr MATH::Vec3 indigo(0.18f, 0.11f, 0.41f);

    /*
    * Four corners per face, so each face has its own UVs. UV (0, 0) is the image's top-left
    * pixel and V grows downwards, so on the four side faces V = 0 sits at the top, y = +0.5
    */
    constexpr Vertex cubeVertices[] = {
        /* -Z face. Seen from outside, +X is to the left */
        {.position = MATH::Vec3(-0.5f, -0.5f, -0.5f), .color = coral, .uv = MATH::Vec2(1.0f, 1.0f)},
        {.position = MATH::Vec3(0.5f, -0.5f, -0.5f), .color = peach, .uv = MATH::Vec2(0.0f, 1.0f)},
        {.position = MATH::Vec3(0.5f, 0.5f, -0.5f), .color = purple, .uv = MATH::Vec2(0.0f, 0.0f)},
        {.position = MATH::Vec3(-0.5f, 0.5f, -0.5f), .color = indigo, .uv = MATH::Vec2(1.0f, 0.0f)},
        /* +Z face, the one facing the camera. Seen from outside, +X is to the right */
        {.position = MATH::Vec3(-0.5f, -0.5f, 0.5f), .color = coral, .uv = MATH::Vec2(0.0f, 1.0f)},
        {.position = MATH::Vec3(0.5f, -0.5f, 0.5f), .color = peach, .uv = MATH::Vec2(1.0f, 1.0f)},
        {.position = MATH::Vec3(0.5f, 0.5f, 0.5f), .color = purple, .uv = MATH::Vec2(1.0f, 0.0f)},
        {.position = MATH::Vec3(-0.5f, 0.5f, 0.5f), .color = indigo, .uv = MATH::Vec2(0.0f, 0.0f)},
        /* +X face. Seen from outside, -Z is to the right */
        {.position = MATH::Vec3(0.5f, -0.5f, -0.5f), .color = peach, .uv = MATH::Vec2(1.0f, 1.0f)},
        {.position = MATH::Vec3(0.5f, -0.5f, 0.5f), .color = peach, .uv = MATH::Vec2(0.0f, 1.0f)},
        {.position = MATH::Vec3(0.5f, 0.5f, 0.5f), .color = purple, .uv = MATH::Vec2(0.0f, 0.0f)},
        {.position = MATH::Vec3(0.5f, 0.5f, -0.5f), .color = purple, .uv = MATH::Vec2(1.0f, 0.0f)},
        /* -X face. Seen from outside, +Z is to the right */
        {.position = MATH::Vec3(-0.5f, -0.5f, 0.5f), .color = coral, .uv = MATH::Vec2(1.0f, 1.0f)},
        {.position = MATH::Vec3(-0.5f, -0.5f, -0.5f), .color = coral, .uv = MATH::Vec2(0.0f, 1.0f)},
        {.position = MATH::Vec3(-0.5f, 0.5f, -0.5f), .color = indigo, .uv = MATH::Vec2(0.0f, 0.0f)},
        {.position = MATH::Vec3(-0.5f, 0.5f, 0.5f), .color = indigo, .uv = MATH::Vec2(1.0f, 0.0f)},
        /* -Y face, the bottom. Seen from below, +X is to the right and +Z is at the top */
        {.position = MATH::Vec3(-0.5f, -0.5f, 0.5f), .color = coral, .uv = MATH::Vec2(0.0f, 0.0f)},
        {.position = MATH::Vec3(0.5f, -0.5f, 0.5f), .color = peach, .uv = MATH::Vec2(1.0f, 0.0f)},
        {.position = MATH::Vec3(0.5f, -0.5f, -0.5f), .color = peach, .uv = MATH::Vec2(1.0f, 1.0f)},
        {.position = MATH::Vec3(-0.5f, -0.5f, -0.5f), .color = coral, .uv = MATH::Vec2(0.0f, 1.0f)},
        /* +Y face, the top. Seen from above, +X is to the right and -Z is at the top */
        {.position = MATH::Vec3(-0.5f, 0.5f, 0.5f), .color = indigo, .uv = MATH::Vec2(0.0f, 1.0f)},
        {.position = MATH::Vec3(-0.5f, 0.5f, -0.5f), .color = indigo, .uv = MATH::Vec2(0.0f, 0.0f)},
        {.position = MATH::Vec3(0.5f, 0.5f, -0.5f), .color = purple, .uv = MATH::Vec2(1.0f, 0.0f)},
        {.position = MATH::Vec3(0.5f, 0.5f, 0.5f), .color = purple, .uv = MATH::Vec2(1.0f, 1.0f)},
    };
    /* Each number is a position in cubeVertices. Every three make one triangle */
    constexpr uint32_t cubeIndices[] = {
        /* -Z face */
        0, 1, 2, 2, 3, 0,
        /* +Z face */
        4, 6, 5, 4, 7, 6,
        /* +X face */
        8, 9, 10, 10, 11, 8,
        /* -X face */
        12, 13, 14, 14, 15, 12,
        /* -Y face */
        16, 17, 18, 18, 19, 16,
        /* +Y face */
        20, 21, 22, 22, 23, 20,
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

TextureHandle ResourceStore::CreateTexture(const std::string& path)
{
    /* The pool has room for a fixed number of sets. Saying so here names the file, where Vulkan would only fail */
    if (textureList.size() == kMaxTextures)
    {
        throw std::runtime_error("cannot create texture " + path + ": the store already holds " +
            std::to_string(kMaxTextures) + " textures");
    }
    /* The new texture's handle is its position in the list, so read the size before adding it */
    const TextureHandle handle{static_cast<uint32_t>(textureList.size())};
    textureList.push_back(std::make_unique<Texture>(gpuContext, temporaryCommandBuffers, path));
    textureSets.push_back(textureDescriptors.AllocateSet(*textureList.back()));
    return handle;
}

VkDescriptorSet ResourceStore::GetTextureSet(const TextureHandle texture) const
{
    assert(texture.index < textureSets.size());
    return textureSets[texture.index];
}
