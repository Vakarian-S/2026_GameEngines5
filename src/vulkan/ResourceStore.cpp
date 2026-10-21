#include "vulkan/ResourceStore.h"
#include "vulkan/ObjLoader.h"

#include <iostream>
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

Material::Material(const GPUContext& gpuContext, const TemporaryCommandBuffers& temporaryCommandBuffers,
                   const MaterialDescriptors& materialDescriptors, const Texture& albedo,
                   const MaterialUniform& values) :
    /* Written once and read every frame, so it goes into GPU-only memory through a staging buffer, like a mesh */
    uniformBuffer(gpuContext, temporaryCommandBuffers, &values, sizeof(MaterialUniform),
                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
    descriptorSet(materialDescriptors.AllocateSet(albedo, uniformBuffer))
{
}

ResourceStore::ResourceStore(const GPUContext& gpuContext_, const TemporaryCommandBuffers& temporaryCommandBuffers_,
                             const MaterialDescriptors& materialDescriptors_) :
    gpuContext(gpuContext_),
    temporaryCommandBuffers(temporaryCommandBuffers_),
    materialDescriptors(materialDescriptors_)
{
}

MeshHandle ResourceStore::CreateCube()
{
    if (cubeCache.has_value()) return cubeCache.value();
    /*
    * Four corners per face, so each face has its own UVs. UV (0, 0) is the image's top-left
    * pixel and V grows downwards, so on the four side faces V = 0 sits at the top, y = +0.5
    */
    constexpr Vertex cubeVertices[] = {
        /* -Z face. Seen from outside, +X is to the left */
        {
            .position = MATH::Vec3(-0.5f, -0.5f, -0.5f), .normal = MATH::Vec3(0.0f, 0.0f, -1.0f),
            .uv = MATH::Vec2(1.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(0.5f, -0.5f, -0.5f), .normal = MATH::Vec3(0.0f, 0.0f, -1.0f),
            .uv = MATH::Vec2(0.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(0.5f, 0.5f, -0.5f), .normal = MATH::Vec3(0.0f, 0.0f, -1.0f),
            .uv = MATH::Vec2(0.0f, 0.0f)
        },
        {
            .position = MATH::Vec3(-0.5f, 0.5f, -0.5f), .normal = MATH::Vec3(0.0f, 0.0f, -1.0f),
            .uv = MATH::Vec2(1.0f, 0.0f)
        },
        /* +Z face, the one facing the camera. Seen from outside, +X is to the right */
        {
            .position = MATH::Vec3(-0.5f, -0.5f, 0.5f), .normal = MATH::Vec3(0.0f, 0.0f, 1.0f),
            .uv = MATH::Vec2(0.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(0.5f, -0.5f, 0.5f), .normal = MATH::Vec3(0.0f, 0.0f, 1.0f),
            .uv = MATH::Vec2(1.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(0.5f, 0.5f, 0.5f), .normal = MATH::Vec3(0.0f, 0.0f, 1.0f),
            .uv = MATH::Vec2(1.0f, 0.0f)
        },
        {
            .position = MATH::Vec3(-0.5f, 0.5f, 0.5f), .normal = MATH::Vec3(0.0f, 0.0f, 1.0f),
            .uv = MATH::Vec2(0.0f, 0.0f)
        },
        /* +X face. Seen from outside, -Z is to the right */
        {
            .position = MATH::Vec3(0.5f, -0.5f, -0.5f), .normal = MATH::Vec3(1.0f, 0.0f, 0.0f),
            .uv = MATH::Vec2(1.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(0.5f, -0.5f, 0.5f), .normal = MATH::Vec3(1.0f, 0.0f, 0.0f),
            .uv = MATH::Vec2(0.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(0.5f, 0.5f, 0.5f), .normal = MATH::Vec3(1.0f, 0.0f, 0.0f),
            .uv = MATH::Vec2(0.0f, 0.0f)
        },
        {
            .position = MATH::Vec3(0.5f, 0.5f, -0.5f), .normal = MATH::Vec3(1.0f, 0.0f, 0.0f),
            .uv = MATH::Vec2(1.0f, 0.0f)
        },
        /* -X face. Seen from outside, +Z is to the right */
        {
            .position = MATH::Vec3(-0.5f, -0.5f, 0.5f), .normal = MATH::Vec3(-1.0f, 0.0f, 0.0f),
            .uv = MATH::Vec2(1.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(-0.5f, -0.5f, -0.5f), .normal = MATH::Vec3(-1.0f, 0.0f, 0.0f),
            .uv = MATH::Vec2(0.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(-0.5f, 0.5f, -0.5f), .normal = MATH::Vec3(-1.0f, 0.0f, 0.0f),
            .uv = MATH::Vec2(0.0f, 0.0f)
        },
        {
            .position = MATH::Vec3(-0.5f, 0.5f, 0.5f), .normal = MATH::Vec3(-1.0f, 0.0f, 0.0f),
            .uv = MATH::Vec2(1.0f, 0.0f)
        },
        /* -Y face, the bottom. Seen from below, +X is to the right and +Z is at the top */
        {
            .position = MATH::Vec3(-0.5f, -0.5f, 0.5f), .normal = MATH::Vec3(0.0f, -1.0f, 0.0f),
            .uv = MATH::Vec2(0.0f, 0.0f)
        },
        {
            .position = MATH::Vec3(0.5f, -0.5f, 0.5f), .normal = MATH::Vec3(0.0f, -1.0f, 0.0f),
            .uv = MATH::Vec2(1.0f, 0.0f)
        },
        {
            .position = MATH::Vec3(0.5f, -0.5f, -0.5f), .normal = MATH::Vec3(0.0f, -1.0f, 0.0f),
            .uv = MATH::Vec2(1.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(-0.5f, -0.5f, -0.5f), .normal = MATH::Vec3(0.0f, -1.0f, 0.0f),
            .uv = MATH::Vec2(0.0f, 1.0f)
        },
        /* +Y face, the top. Seen from above, +X is to the right and -Z is at the top */
        {
            .position = MATH::Vec3(-0.5f, 0.5f, 0.5f), .normal = MATH::Vec3(0.0f, 1.0f, 0.0f),
            .uv = MATH::Vec2(0.0f, 1.0f)
        },
        {
            .position = MATH::Vec3(-0.5f, 0.5f, -0.5f), .normal = MATH::Vec3(0.0f, 1.0f, 0.0f),
            .uv = MATH::Vec2(0.0f, 0.0f)
        },
        {
            .position = MATH::Vec3(0.5f, 0.5f, -0.5f), .normal = MATH::Vec3(0.0f, 1.0f, 0.0f),
            .uv = MATH::Vec2(1.0f, 0.0f)
        },
        {
            .position = MATH::Vec3(0.5f, 0.5f, 0.5f), .normal = MATH::Vec3(0.0f, 1.0f, 0.0f),
            .uv = MATH::Vec2(1.0f, 1.0f)
        },
    };
    /* Each number is a position in cubeVertices. Every three make one triangle */
    constexpr uint32_t cubeIndices[] = {
        /* -Z face */
        0, 2, 1, 2, 0, 3,
        /* +Z face */
        4, 5, 6, 4, 6, 7,
        /* +X face */
        8, 10, 9, 10, 8, 11,
        /* -X face */
        12, 14, 13, 14, 12, 15,
        /* -Y face */
        16, 18, 17, 18, 16, 19,
        /* +Y face */
        20, 22, 21, 22, 20, 23,
    };
    /* The new mesh's handle is its position in the list, so read the size before adding it */
    const MeshHandle handle{static_cast<uint32_t>(meshList.size())};
    /* Both arrays are copied into GPU memory once, here. The frame loop only reads them */
    meshList.push_back(std::make_unique<Mesh>(gpuContext, temporaryCommandBuffers, cubeVertices, cubeIndices));
    cubeCache = handle;
    return handle;
}

MeshHandle ResourceStore::CreateMesh(const std::string& path)
{
    if (const auto it = meshCache.find(path); it != meshCache.end()) return it->second;
    /* The CPU copy lives only until the GPU copy exists */
    const auto [vertices, indices] = LoadObjFile(path);
    const MeshHandle handle{static_cast<uint32_t>(meshList.size())};
    meshList.push_back(std::make_unique<Mesh>(gpuContext, temporaryCommandBuffers, vertices,
                                              indices));
    meshCache.emplace(path, handle);
    return handle;
}

const Mesh& ResourceStore::GetMesh(const MeshHandle mesh) const
{
    assert(mesh.index < meshList.size());
    return *meshList[mesh.index];
}

TextureHandle ResourceStore::CreateTexture(const std::string& path)
{
    if (const auto it = textureCache.find(path); it != textureCache.end()) return it->second;
    /* The new texture's handle is its position in the list, so read the size before adding it */
    const TextureHandle handle{static_cast<uint32_t>(textureList.size())};
    textureList.push_back(std::make_unique<Texture>(gpuContext, temporaryCommandBuffers, path));
    textureCache.emplace(path, handle);
    return handle;
}

MaterialHandle ResourceStore::CreateMaterial(const MaterialDesc& desc)
{
    for (const auto& [materialDescription, materialHandle] : materialCache)
    {
        if (materialDescription.shininess == desc.shininess &&
            materialDescription.specularStrength == desc.specularStrength &&
            materialDescription.albedo.index == desc.albedo.index)
        {
            return materialHandle;
        }
    }

    /* The pool has room for a fixed number of sets. Saying so here gives the reason, where Vulkan would only fail */
    if (materialList.size() == kMaxMaterials)
    {
        throw std::runtime_error("cannot create a material: the store already holds " +
            std::to_string(kMaxMaterials) + " materials");
    }
    assert(desc.albedo.index < textureList.size());

    const MaterialUniform values{.shininess = desc.shininess, .specularStrength = desc.specularStrength};
    /* The new material's handle is its position in the list, so read the size before adding it */
    const MaterialHandle handle{static_cast<uint32_t>(materialList.size())};
    materialList.push_back(std::make_unique<Material>(gpuContext, temporaryCommandBuffers, materialDescriptors,
                                                      *textureList[desc.albedo.index], values));
    materialCache.emplace_back(desc, handle);
    return handle;
}

VkDescriptorSet ResourceStore::GetMaterialSet(const MaterialHandle material) const
{
    assert(material.index < materialList.size());
    return materialList[material.index]->descriptorSet;
}
