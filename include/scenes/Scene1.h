#pragma once
#include <array>
#include <span>

#include "core/Camera.h"
#include "core/FlyCameraController.h"
#include "core/Handles.h"
#include "core/Scene.h"
#include "ecs/Query.h"
#include "ecs/SystemRegistry.h"
#include "ecs/World.h"

/**
 * The light and shadow scene: Eirene stands in a ruined temple on a raised base, like a diorama. Only
 * the back wall, with one window, still stands. The sides are rows of columns, so the side lights cast
 * column shadows across the floor. Every entity carries its own mesh and material in a Renderable.
 */
class Scene1 : public Scene
{
public:
    Scene1();
    ~Scene1() override = default;

    bool OnCreate(AssetLoader& assetLoader) override;
    void OnDestroy() override;
    void Update(float deltaTime, const InputState& input) override;
    void HandleEvents(const SDL_Event& sdlEvent) override;

    /** @return the camera this scene is seen through */
    const Camera& GetCamera() const override { return camera; }

    /** @return the ambient light and the lights of the scheme the number keys picked */
    SceneLighting GetLighting() const override
    {
        const LightingScheme& scheme = kSchemes[activeScheme];
        return {.ambient = scheme.ambient, .lights = std::span<const Light>(scheme.lights.data(), scheme.count)};
    }

    /** @return this scene's world, so Main can read its counters */
    World& GetWorld() override { return world; }

    /** @return this scene's systems, so the frame loop can run their phases */
    SystemRegistry& GetSystems() override { return systems; }

    /**
     * @return Struct containing the Cubemap Handle on the scene, which is the skybox, it can be null as scenes
     * could not have a skybox
     */
    CubeMapHandle GetSky() const override { return skyboxSchemes[activeScheme]; }

private:
    /** Most entities alive at once: Eirene, 8 columns, 4 rubble blocks and 10 base, step, wall and beam boxes, with room to spare */
    static constexpr uint32_t kMaxEntities = 32;
    /** Bytes in Scene1's world slab: one 16 KB chunk per archetype with rows, one today, plus the slots and archetypes */
    static constexpr std::size_t kWorldArenaBytes = 64 * 1024;

    World world;
    /** Every system this scene runs */
    SystemRegistry systems;
    /** In front of the ruin, looking at Eirene's middle */
    Camera camera;

    /** One lighting setup. The number keys switch between them. */
    struct LightingScheme
    {
        /** Printed when the scheme is picked. */
        const char* name;
        /** Light that reaches every surface. */
        MATH::Vec3 ambient;
        /** Only the first count entries are used. Four is the most the renderer takes */
        std::array<Light, 4> lights;
        /** How many entries of lights are lit. */
        uint32_t count;
        /** Skybox Folder or nullptr if the scene has no sky */
        const char* skybox = nullptr;
    };

    /** A dim light above the middle of the ruin, in every scheme. It lifts the dark corners and casts a soft shadow under her */
    static constexpr Light kCeilingLight{
        .position = MATH::Vec3(0.0f, 3.9f, 0.0f), .color = MATH::Vec3(0.08f, 0.08f, 0.09f)
    };

    /**
     * The fixed schemes keys 1 to 4 pick. Every coloured light sits outside the ruin: the side lights
     * shine between the columns, and the back light only through the back window. Light colours add up
     * and clip at 1. Lights do not fade with distance, but the shadow ray ends at the light.
     */
    static constexpr std::array<LightingScheme, 4> kSchemes{
        {
            /* 1. A deep orange, low sun outside the left window, with a purple ambient for the shadow side */
            {
                .name = "Sunset",
                .ambient = MATH::Vec3(0.05f, 0.02f, 0.05f),
                .lights = {
                    {
                        {.position = MATH::Vec3(-12.0f, 3.5f, 1.5f), .color = MATH::Vec3(1.0f, 0.42f, 0.08f)},
                        kCeilingLight,
                    }
                },
                .count = 2,
                .skybox = "assets/skyboxes/sunset",
            },
            /* 2. A strong blue moon outside the right window */
            {
                .name = "Moonlight",
                .ambient = MATH::Vec3(0.01f, 0.015f, 0.04f),
                .lights = {
                    {
                        {.position = MATH::Vec3(12.0f, 3.5f, -4.0f), .color = MATH::Vec3(0.15f, 0.3f, 1.0f)},
                        kCeilingLight,
                    }
                },
                .count = 2,
                .skybox = "assets/skyboxes/night",
            },
            /* 3. A deep red light behind the back window, to match the red skies: she shows as a silhouette and her shadow falls toward the camera */
            {
                .name = "Backlight",
                .ambient = MATH::Vec3(0.04f, 0.01f, 0.01f),
                .lights = {
                    {
                        {.position = MATH::Vec3(0.0f, 3.0f, -14.0f), .color = MATH::Vec3(1.0f, 0.15f, 0.05f)},
                        kCeilingLight,
                    }
                },
                .count = 2,
                .skybox = "assets/skyboxes/red_skies",
            },
            /* 4. Red through the left window, green through the right, blue through the back. Overlaps mix */
            {
                .name = "RGB windows",
                .ambient = MATH::Vec3(0.02f, 0.02f, 0.02f),
                .lights = {
                    {
                        {.position = MATH::Vec3(-12.0f, 3.5f, 0.0f), .color = MATH::Vec3(1.0f, 0.0f, 0.0f)},
                        {.position = MATH::Vec3(12.0f, 3.5f, 0.0f), .color = MATH::Vec3(0.0f, 1.0f, 0.0f)},
                        {.position = MATH::Vec3(0.0f, 3.5f, -14.0f), .color = MATH::Vec3(0.0f, 0.0f, 1.0f)},
                        kCeilingLight,
                    }
                },
                .count = 4,
            },
        }
    };

    /** Array of the skyboxes used in the scene */
    std::array<CubeMapHandle, kSchemes.size()> skyboxSchemes;

    /** The position in kSchemes of the scheme in use. Starts on Sunset */
    uint32_t activeScheme = 0;

    MeshHandle EireneMesh;
    MeshHandle columnMesh;
    MeshHandle cubeMesh;

    /** The columns' own marble texture */
    MaterialHandle columnMaterial;
    /** Eirene's stone: the floor texture with a little more shine */
    MaterialHandle statueMaterial;
    /** The floor, walls and ceiling */
    MaterialHandle floorMaterial;

    /** Controller to move the camera with user input */
    FlyCameraController flyCameraController;
};
