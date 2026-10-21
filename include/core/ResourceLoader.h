#pragma once

#include "core/Handles.h"
#include "core/Material.h"

#include <string>

/**
 * The contract that lets a scene load assets using a renderer's loader, in our case currently the Vulkan Loader
 */
class ResourceLoader
{
public:
    virtual ~ResourceLoader() = default;
    
    /**
     * Copies a unit cube into GPU memory.
     * @return the handle that names the cube in draw items
     */
    virtual MeshHandle CreateCube() = 0;
    
    /**
    * Reads an OBJ file and copies its mesh into GPU memory.
    * @param path the OBJ file, as a path next to the executable, such as "assets/meshes/Mario.obj"
    * @return the handle that names the mesh in draw items
    */
    virtual MeshHandle CreateMesh(const std::string& path) = 0;
    
    /**
    * Loads an image file into GPU memory.
    * @param path the image file, as a path next to the executable, such as "assets/textures/skull_texture.jpg"
    * @return the handle that names the texture in a MaterialDesc
    */
    virtual TextureHandle CreateTexture(const std::string& path) = 0;
    
    /**
     * Copies a material's surface values into GPU memory and allocates the set that points at them and at its texture.
     * @param desc the texture and the surface values. The texture must come from this store
     * @return the handle that names the material in draw items
     */
    virtual MaterialHandle CreateMaterial(const MaterialDesc& desc) = 0;
};
