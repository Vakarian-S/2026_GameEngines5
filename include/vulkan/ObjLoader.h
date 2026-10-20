#pragma once
#include <string>
#include <vector>

#include "Vertex.h"

/** One mesh in CPU memory, ready to copy into a vertex buffer and an index buffer. */
struct MeshData
{
    /** Every distinct corner. Corners that share a position, a normal and a UV are stored once. */
    std::vector<Vertex> vertices;
    /** Positions in vertices. Every three make one triangle, wound counter-clockwise. */
    std::vector<uint32_t> indices;
};

/**
 * Reads an OBJ file into CPU memory. It makes no Vulkan calls, so it runs without a GPU.
 * @param path the OBJ file, as a path next to the executable, such as "assets/meshes/Mario.obj"
 * @return the corners and the triangles. V is flipped, because OBJ puts V = 0 at the bottom of the image
 * @throws std::runtime_error if the file cannot be read, or a corner has no normal or no UV
 */
MeshData LoadObjFile(const std::string& path);
