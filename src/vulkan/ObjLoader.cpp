#include "vulkan/ObjLoader.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <tiny_obj_loader.h>
#include <unordered_map>

namespace
{
    /** Two corners are the same corner only when all eight floats match exactly. */
    struct VertexEqual
    {
        bool operator()(const Vertex& a, const Vertex& b) const
        {
            return a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z &&
                a.normal.x == b.normal.x && a.normal.y == b.normal.y && a.normal.z == b.normal.z &&
                a.uv.x == b.uv.x && a.uv.y == b.uv.y;
        }
    };

    /** Mixes the hash of each of the eight floats into one number, so the map can find a corner quickly. */
    struct VertexHash
    {
        std::size_t operator()(const Vertex& vertex) const
        {
            const float values[] = {
                vertex.position.x, vertex.position.y, vertex.position.z,
                vertex.normal.x, vertex.normal.y, vertex.normal.z,
                vertex.uv.x, vertex.uv.y
            };
            std::size_t seed = 0;
            for (const float value : values)
            {
                /* The constant and the shifts spread each float's bits across the whole seed */
                seed ^= std::hash<float>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}

MeshData LoadObjFile(const std::string& path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warning;
    std::string error;
    
    /* Faces with more than three corners are split into triangles. The .mtl file, if any, is ignored */
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, path.c_str()))
    {
        throw std::runtime_error("failed to load mesh " + path + ": " + warning + error);
    }
    
    /* A warning does not stop the load, so print it rather than drop it */
    if (!warning.empty())
    {
        std::cerr << "warning while loading mesh " << path << ": " << warning << std::endl;
    }
    
    MeshData meshData;
    /* Each corner number the file uses maps to its position in meshData.vertices */
    std::unordered_map<Vertex, uint32_t, VertexHash, VertexEqual> uniqueVertices;
    
    for (const tinyobj::shape_t& shape : shapes)
    {
        for (const auto& [vertex_index, normal_index, texcoord_index] : shape.mesh.indices)
        {
            /* -1 means the face gave this corner no normal or no UV. The lighting needs both */
            if (normal_index < 0 || texcoord_index < 0)
            {
                throw std::runtime_error("mesh " + path + " has a corner without a normal or a UV");
            }

            const Vertex vertex{
                .position = MATH::Vec3(attrib.vertices[3 * vertex_index + 0],
                                       attrib.vertices[3 * vertex_index + 1],
                                       attrib.vertices[3 * vertex_index + 2]),
                .normal = MATH::Vec3(attrib.normals[3 * normal_index + 0],
                                     attrib.normals[3 * normal_index + 1],
                                     attrib.normals[3 * normal_index + 2]),
                /* OBJ puts V = 0 at the bottom of the image, and Vulkan puts it at the top */
                .uv = MATH::Vec2(attrib.texcoords[2 * texcoord_index + 0],
                                 1.0f - attrib.texcoords[2 * texcoord_index + 1])
            };

            /* A corner seen before reuses its number. A new one is added at the end */
            const auto [entry, inserted] = uniqueVertices.try_emplace(
                vertex, static_cast<uint32_t>(meshData.vertices.size()));
            if (inserted)
            {
                meshData.vertices.push_back(vertex);
            }
            meshData.indices.push_back(entry->second);
        }
    }
    return meshData;
}
