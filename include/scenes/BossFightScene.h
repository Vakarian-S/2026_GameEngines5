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
 * The boss fight: a boss on a pattern and a player that moves and shoots
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

    /** @return the boss fight scene's ambient light and its one light */
    SceneLighting GetLighting() const override
    {
        return {.ambient = kAmbient, .lights = std::span<const Light>(kLights.data(), kLights.size())};
    }

    /** @return the boss fight scene's world, so Main can read its counters */
    World& GetWorld() override { return world; }
    /** @return the boss fight scene's systems, so the frame loop can run their phases */
    SystemRegistry& GetSystems() override { return systems; }

private:
    /** Light that reaches every surface in the boss fight scene */
    static constexpr MATH::Vec3 kAmbient = MATH::Vec3(0.1f, 0.1f, 0.12f);
    /** The boss fight scene's one light, high above the arena and a little in front */
    static constexpr std::array<Light, 1> kLights{
        {{.position = MATH::Vec3(0.0f, 10.0f, 5.0f), .color = MATH::Vec3(0.9f, 0.9f, 0.85f)}}
    };

    World world;
    /** Every system this scene runs */
    SystemRegistry systems;
    /** Camera for the scene */
    Camera camera;
    
    /** RenderableComponent for the player */
    RenderableComponent playerRenderable;
    /** RenderableComponent for the boss */
    RenderableComponent bossRenderable;
    /** RenderableComponent for the projectiles launched by the player */
    RenderableComponent projectileRenderable;
    /** RenderableComponent for the Arena floor */
    RenderableComponent floorRenderable;
    
    /** Spawns all the entities on the scene */
    void SpawnEntities();
};
