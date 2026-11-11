#pragma once
#include <array>
#include <span>

#include "core/Camera.h"
#include "core/FlyCameraController.h"
#include "core/Light.h"
#include "core/Scene.h"
#include "ecs/SystemRegistry.h"
#include "ecs/World.h"

class LoadSystem;

/**
 * A test scene for frame pacing: two rows of Marios slide right and wrap around. The top row moves
 * once per frame, the bottom row once per fixed step. Ctrl+H cycles a fake CPU load.
 */
class PacingScene : public Scene
{
public:
    PacingScene();
    ~PacingScene() override = default;

    bool OnCreate(ResourceLoader& resourceLoader) override;
    void OnDestroy() override;
    void Update(float deltaTime, const InputState& input) override;
    void HandleEvents(const SDL_Event& sdlEvent) override;

    /** @return the camera the pacing scene is seen through */
    const Camera& GetCamera() const override { return camera; }

    /** @return the pacing scene's ambient light and its one light */
    SceneLighting GetLighting() const override
    {
        return {.ambient = kAmbient, .lights = std::span<const Light>(kLights.data(), kLights.size())};
    }

    /** @return the pacing scene's world, so Main can read its counters */
    const World& GetWorld() const override { return world; }

    /** @return the pacing scene's systems, so the frame loop can run their phases */
    SystemRegistry& GetSystems() override { return systems; }

private:
    /** The fake loads Ctrl+H cycles through */
    enum class LoadStep { Off, Every5Ms, Every7_5Ms, Spike25MsEvery30, Count };

    /** Moves to the next load step, sets the load system and prints it */
    void CycleLoad();

    /** Most entities alive at once in the pacing scene: two rows of 10, with room to spare */
    static constexpr uint32_t kMaxEntities = 32;
    /** Bytes in the pacing scene's world slab */
    static constexpr std::size_t kWorldArenaBytes = 64 * 1024;
    /** Marios in each row */
    static constexpr int kMariosPerRow = 10;
    /** Where the slide path starts, in metres. The path is wider than the view, so the wrap happens off screen */
    static constexpr float kPathLeft = -15.0f;
    /** The slide path's width, in metres */
    static constexpr float kPathWidth = 30.0f;
    /** How fast both rows slide right, in metres per second */
    static constexpr float kSpeed = 10.0f;
    /** How far above and below the middle each row sits, in metres */
    static constexpr float kRowHeight = 2.0f;

    /** Light that reaches every surface in the pacing scene */
    static constexpr MATH::Vec3 kAmbient = MATH::Vec3(0.1f, 0.1f, 0.12f);
    /** The pacing scene's one light, in front and above */
    static constexpr std::array<Light, 1> kLights{
        {{.position = MATH::Vec3(2.0f, 4.0f, 10.0f), .color = MATH::Vec3(0.9f, 0.9f, 0.85f)}}
    };

    World world;
    /** Every system the pacing scene runs */
    SystemRegistry systems;
    /** The pacing scene's camera, set from the fly camera each frame */
    Camera camera;
    /** Moves the pacing scene's camera with user input */
    FlyCameraController flyCameraController;
    /** The load system, kept so Ctrl+H can change its load. The registry owns it */
    LoadSystem* loadSystem = nullptr;
    /** The load step in use */
    LoadStep loadStep = LoadStep::Off;
};