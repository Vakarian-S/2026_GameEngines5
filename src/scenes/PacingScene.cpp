#include "scenes/PacingScene.h"

#include <iostream>
#include <SDL3/SDL.h>

#include "core/Components.h"
#include "core/ResourceLoader.h"
#include "core/systems/DrawRenderablesSystem.h"
#include "game/Components.h"
#include "game/systems/LoadSystem.h"
#include "game/systems/SlideRightSystem.h"
#include "physics/Components.h"
#include "utils/Profiler.h"

PacingScene::PacingScene() :
    world(WorldConfig{kMaxEntities, kWorldArenaBytes}),
    systems(world),
    flyCameraController(MATH::Vec3(0.0f, 0.0f, 14.0f), MATH::Vec3(0.0f, 0.0f, 0.0f))
{
}

bool PacingScene::OnCreate(ResourceLoader& resourceLoader)
{
    PROFILE_SCOPE_ECS(world, "OnCreate");

    const TextureHandle marioTexture = resourceLoader.CreateTexture("assets/textures/mario_main.png");
    const MeshHandle marioMesh = resourceLoader.CreateMesh("assets/meshes/Mario.obj");
    const MaterialHandle marioMaterial = resourceLoader.CreateMaterial({
        .albedo = marioTexture, .shininess = 32.0f, .specularStrength = 0.5f
    });

    /* Both rows start in the same places along the path, so any gap between them is the pacing */
    const PHYSICS::Velocity velocity{MATH::Vec3(kSpeed, 0.0f, 0.0f)};
    for (int i = 0; i < kMariosPerRow; ++i)
    {
        const float x = kPathLeft + kPathWidth * static_cast<float>(i) / kMariosPerRow;
        world.CreateEntity(Transform{.position = MATH::Vec3(x, kRowHeight, 0.0f)},
                           Renderable{marioMesh, marioMaterial}, velocity, FrameStepRow{});
        world.CreateEntity(Transform{.position = MATH::Vec3(x, -kRowHeight, 0.0f)},
                           Renderable{marioMesh, marioMaterial}, velocity, FixedStepRow{});
    }

    systems.Add(Phase::Fixed, std::make_unique<SlideRightSystem>(
                    world, "SlideFixedStepRow", COMPONENTS::MaskOf<FixedStepRow>(), kPathLeft, kPathWidth));
    systems.Add(Phase::Update, std::make_unique<SlideRightSystem>(
                    world, "SlideFrameStepRow", COMPONENTS::MaskOf<FrameStepRow>(), kPathLeft, kPathWidth));
    auto load = std::make_unique<LoadSystem>();
    loadSystem = load.get();
    systems.Add(Phase::Update, std::move(load));
    systems.Add(Phase::Extract, std::make_unique<DrawRenderablesSystem>(world));

    std::cout << "pacing scene: top row moves per frame, bottom row per fixed step. Ctrl+H cycles the load\n";
    return true;
}

void PacingScene::OnDestroy()
{
    /* Folds the final frame into the run totals, which Main prints */
    world.ResetFrameStats();
}

void PacingScene::Update(const float deltaTime, const InputState&)
{
    world.ResetFrameStats();
    flyCameraController.Update(deltaTime);
    camera.view = flyCameraController.View();
}

void PacingScene::HandleEvents(const SDL_Event& sdlEvent)
{
    /* Ctrl+H cycles the fake load */
    if (sdlEvent.type == SDL_EVENT_KEY_DOWN && !sdlEvent.key.repeat && sdlEvent.key.scancode == SDL_SCANCODE_H &&
        (sdlEvent.key.mod & SDL_KMOD_CTRL) != 0)
    {
        CycleLoad();
        return;
    }
    flyCameraController.HandleEvent(sdlEvent);
}

void PacingScene::CycleLoad()
{
    constexpr int count = static_cast<int>(LoadStep::Count);
    loadStep = static_cast<LoadStep>((static_cast<int>(loadStep) + 1) % count);
    switch (loadStep)
    {
    case LoadStep::Every5Ms:
        loadSystem->SetLoad(5.0, 0);
        std::cout << "load: 5 ms every frame\n";
        break;
    case LoadStep::Every7_5Ms:
        loadSystem->SetLoad(7.5, 0);
        std::cout << "load: 7.5 ms every frame\n";
        break;
    case LoadStep::Spike25MsEvery30:
        loadSystem->SetLoad(25.0, 30);
        std::cout << "load: 25 ms every 30 frames\n";
        break;
    case LoadStep::Off:
    case LoadStep::Count:
        loadSystem->SetLoad(0.0, 0);
        std::cout << "load: off\n";
        break;
    }
}