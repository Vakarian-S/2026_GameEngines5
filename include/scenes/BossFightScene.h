#pragma once
#include "core/Camera.h"
#include "core/Scene.h"
#include "ecs/SystemRegistry.h"
#include "ecs/World.h"
#include "core/Light.h"

#include <array>
#include <span>

#include "core/Components.h"

/**
 * The boss fight: a boss on a pattern and a player that moves and shoots, in a walled arena at night
 */
class BossFightScene : public Scene
{
public:
    BossFightScene();
    ~BossFightScene() override = default;
    void Update(float deltaTime, const InputState& input) override;
    bool OnCreate(AssetLoader& assetLoader) override;
    void OnDestroy() override;
    void HandleEvents(const SDL_Event& sdlEvent) override;

    /** @return the camera the boss fight scene is seen through */
    const Camera& GetCamera() const override { return camera; }

    /** @return the boss fight scene's moonlight ambient, the moon and the two torches */
    SceneLighting GetLighting() const override
    {
        return {.ambient = kAmbient, .lights = std::span<const Light>(kLights.data(), kLights.size())};
    }

    /** @return the boss fight scene's night skybox */
    CubeMapHandle GetSky() const override { return sky; }

    /** @return the boss fight scene's world, so Main can read its counters */
    World& GetWorld() override { return world; }
    /** @return the boss fight scene's systems, so the frame loop can run their phases */
    SystemRegistry& GetSystems() override { return systems; }

private:
    /** Light that reaches every surface in the boss fight scene. */
    static constexpr MATH::Vec3 kAmbient = MATH::Vec3(0.01f, 0.015f, 0.04f);
    /** The boss fight scene's lights: a blue moon far off to the right, and a warm light just in front of each torch on the back wall */
    static constexpr std::array<Light, 3> kLights{
        {
            {.position = MATH::Vec3(22.0f, 8.0f, -8.0f), .color = MATH::Vec3(0.15f, 0.3f, 1.0f)},
            {.position = MATH::Vec3(-6.0f, 0.6f, -9.6f), .color = MATH::Vec3(0.5f, 0.25f, 0.06f)},
            {.position = MATH::Vec3(6.0f, 0.6f, -9.6f), .color = MATH::Vec3(0.5f, 0.25f, 0.06f)},
        }
    };

    World world;
    /** Every system this scene runs */
    SystemRegistry systems;
    /** Camera for the scene */
    Camera camera;
    /** The boss fight scene's night skybox */
    CubeMapHandle sky;
    /** The boss fight scene's collider wires. Off at the start; C turns them on and off */
    System* drawColliders = nullptr;

    /** RenderableComponent for the player */
    RenderableComponent playerRenderable;
    /** RenderableComponent for the boss */
    RenderableComponent bossRenderable;
    /** RenderableComponent for the projectiles launched by the player. Projectiles cast no shadow */
    RenderableComponent projectileRenderable;
    /** RenderableComponent for the Arena floor */
    RenderableComponent floorRenderable;
    /** RenderableComponent for the low walls around the arena */
    RenderableComponent wallRenderable;
    /** RenderableComponent for the ionic columns on top of the arena walls */
    RenderableComponent columnRenderable;
    /** RenderableComponent for the torches on the back wall, plain cubes until a torch model exists */
    RenderableComponent torchRenderable;

    /** Spawns all the entities on the scene */
    void SpawnEntities();
    /** Spawns the floor the boss fight happens on */
    void SpawnFloor();
    /** Spawns a low wall along each edge of the arena floor, just outside the arena clamp */
    void SpawnWalls();
    /** Spawns the ionic columns on top of the arena walls */
    void SpawnColumns();
    /** Spawns the two torches on top of the arena's back wall */
    void SpawnTorches();
    /** Spawns the player, who moves and shoots */
    void SpawnPlayer();
    /** Spawns the boss on its figure-eight path */
    void SpawnBoss();
};