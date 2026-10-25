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
 * The milestone 8 scene: a lit, turning Mario between two matte cubes. Every entity carries its
 * own mesh and material in a Renderable, so MakeDrawList reads them from the world.
 */
class Scene1 : public Scene
{
public:
    Scene1();
    ~Scene1() override = default;

    bool OnCreate(ResourceLoader& resourceLoader) override;
    void OnDestroy() override;
    void Update(float deltaTime) override;
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
    const World& GetWorld() const override { return world; }
    
    /** @return this scene's systems, so the frame loop can run their phases */
    SystemRegistry& GetSystems() override { return systems; }

private:
   
    
    /** Most entities alive at once: Mario and two cubes, with room to spare */
    static constexpr uint32_t kMaxEntities = 16;
    /** Bytes in the world's slab. Each archetype reserves kMaxEntities rows, so a few hundred bytes each */
    static constexpr std::size_t kWorldArenaBytes = 16 * 1024;

    World world;
    /** Every system this scene runs */
    SystemRegistry systems;
    /** In front of Mario and a little above, looking at his middle */
    Camera camera;

    /** One lighting setup. The number keys switch between them. */
    struct LightingScheme
    {
        /** Printed when the scheme is picked. */
        const char* name;
        /** Light that reaches every surface. */
        MATH::Vec3 ambient;
        /** Only the first count entries are used. */
        std::array<Light, 3> lights;
        /** How many entries of lights are lit. */
        uint32_t count;
    };

    /**
     * The schemes keys 1 to 5 pick. Light colours add up and clip at 1, so each scheme keeps its
     * total near 1. Lights do not fade with distance, so a position only sets a direction.
     */
    static constexpr std::array<LightingScheme, 5> kSchemes{
        {
            /* 1. Film lighting: a warm key light, a dim cool fill on the other side, and a rim light behind that outlines him */
            {
                .name = "Studio",
                .ambient = MATH::Vec3(0.05f, 0.05f, 0.06f),
                .lights = {
                    {
                        {.position = MATH::Vec3(3.0f, 4.0f, 5.0f), .color = MATH::Vec3(0.9f, 0.85f, 0.75f)},
                        {.position = MATH::Vec3(-4.0f, 1.0f, 3.0f), .color = MATH::Vec3(0.15f, 0.18f, 0.25f)},
                        {.position = MATH::Vec3(0.0f, 3.0f, -5.0f), .color = MATH::Vec3(0.6f, 0.6f, 0.7f)},
                    }
                },
                .count = 3,
            },
            /* 2. A low orange sun on the right and a weak blue sky above, so his shadow side turns blue rather than grey */
            {
                .name = "Sunset",
                .ambient = MATH::Vec3(0.08f, 0.04f, 0.1f),
                .lights = {
                    {
                        {.position = MATH::Vec3(6.0f, 0.5f, 1.0f), .color = MATH::Vec3(1.0f, 0.5f, 0.15f)},
                        {.position = MATH::Vec3(-2.0f, 6.0f, 2.0f), .color = MATH::Vec3(0.1f, 0.15f, 0.4f)},
                    }
                },
                .count = 2,
            },
            /* 3. Magenta from the left and cyan from the right split him down the middle */
            {
                .name = "Neon",
                .ambient = MATH::Vec3(0.02f, 0.02f, 0.04f),
                .lights = {
                    {
                        {.position = MATH::Vec3(-4.0f, 1.0f, 3.0f), .color = MATH::Vec3(0.9f, 0.1f, 0.6f)},
                        {.position = MATH::Vec3(4.0f, 1.0f, 3.0f), .color = MATH::Vec3(0.1f, 0.8f, 0.9f)},
                    }
                },
                .count = 2,
            },
            /* 4. Pure red, green and blue from three sides. Where two overlap they add to yellow, cyan or magenta */
            {
                .name = "RGB mixing",
                .ambient = MATH::Vec3(0.03f, 0.03f, 0.03f),
                .lights = {
                    {
                        {.position = MATH::Vec3(-4.0f, 2.0f, 3.0f), .color = MATH::Vec3(1.0f, 0.0f, 0.0f)},
                        {.position = MATH::Vec3(4.0f, 2.0f, 3.0f), .color = MATH::Vec3(0.0f, 1.0f, 0.0f)},
                        {.position = MATH::Vec3(0.0f, -3.0f, 3.0f), .color = MATH::Vec3(0.0f, 0.0f, 1.0f)},
                    }
                },
                .count = 3,
            },
            /* 5. One orange light low in front and almost no ambient: the flashlight under the chin */
            {
                .name = "Campfire",
                .ambient = MATH::Vec3(0.02f, 0.01f, 0.01f),
                .lights = {
                    {
                        {.position = MATH::Vec3(0.0f, -2.0f, 2.0f), .color = MATH::Vec3(1.0f, 0.45f, 0.1f)},
                    }
                },
                .count = 1,
            },
        }
    };

    /** The position in kSchemes of the scheme in use. Starts on Studio */
    uint32_t activeScheme = 0;

    MeshHandle marioMesh;
    MaterialHandle marioMaterial;
    MeshHandle cubeMesh;
    MaterialHandle cubeMaterial;
    
    /** Controller to move the camera with user input */
    FlyCameraController flyCameraController;
};
