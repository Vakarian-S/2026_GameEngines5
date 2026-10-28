#pragma once
#include <array>

#include "core/Camera.h"
#include "core/FlyCameraController.h"
#include "core/Handles.h"
#include "core/Scene.h"
#include "ecs/SystemRegistry.h"
#include "ecs/World.h"

class PhysicsScene : public Scene
{
public:
    PhysicsScene();
    ~PhysicsScene() override = default;

    bool OnCreate(ResourceLoader& resourceLoader) override;
    void OnDestroy() override;
    void Update(float deltaTime) override;
    void HandleEvents(const SDL_Event& sdlEvent) override;

    /** @return the camera this scene is seen through */
    const Camera& GetCamera() const override { return camera; }

    /** @return the ambient light and the lights of the current scene */
    SceneLighting GetLighting() const override
    {
        return {
            .ambient = MATH::Vec3(0.05f, 0.05f, 0.06f),
            .lights = kLights
        };
    }

    /** @return this scene's world, so Main can read its counters */
    const World& GetWorld() const override { return world; }

    /** @return this scene's systems, so the frame loop can run their phases */
    SystemRegistry& GetSystems() override { return systems; }

private:
    /** The three lights of this scene: a warm key light, a cool fill, and a rim light behind */
    static constexpr std::array<Light, 3> kLights{
        {
            {.position = MATH::Vec3(3.0f, 4.0f, 5.0f), .color = MATH::Vec3(0.9f, 0.85f, 0.75f)},
            {.position = MATH::Vec3(-4.0f, 1.0f, 3.0f), .color = MATH::Vec3(0.15f, 0.18f, 0.25f)},
            {.position = MATH::Vec3(0.0f, 3.0f, -5.0f), .color = MATH::Vec3(0.6f, 0.6f, 0.7f)},
        }
    };

    /** Most entities alive at once */
    static constexpr uint32_t kMaxEntities = 16;
    /** Bytes in the world's slab. Each archetype reserves kMaxEntities rows, so a few hundred bytes each */
    static constexpr std::size_t kWorldArenaBytes = 16 * 1024;

    World world;
    /** Every system this scene runs */
    SystemRegistry systems;
    /** Camera for the scene */
    Camera camera;

    /** Controller to move the camera with user input */
    FlyCameraController flyCameraController;

    MeshHandle marioMesh;
    MaterialHandle marioMaterial;
    MeshHandle cubeMesh;
    MaterialHandle cubeMaterial;
};
