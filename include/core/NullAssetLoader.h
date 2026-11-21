#pragma once
#include "AssetLoader.h"

/**
 * Asset Loader implementation that returns only null handles, meant to be
 * used for a headless application
 */
class NullAssetLoader : public AssetLoader
{
public:
    NullAssetLoader() = default;

    MeshHandle CreateCube() override { return {}; }
    MeshHandle CreateMesh(const std::string&) override { return {}; }
    TextureHandle CreateTexture(const std::string&) override { return {}; }
    MaterialHandle CreateMaterial(const MaterialDesc&) override { return {}; }
    CubeMapHandle CreateCubemap(const std::string&) override { return {}; }
};
