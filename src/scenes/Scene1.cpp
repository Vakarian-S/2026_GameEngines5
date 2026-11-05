#include "scenes/Scene1.h"
#include <iostream>
#include <SDL3/SDL.h>

#include "MMath.h"
#include "core/Components.h"
#include "core/ResourceLoader.h"
#include "core/systems/DrawRenderablesSystem.h"
#include "physics/Components.h"
#include "physics/systems/DrawCollidersSystem.h"
#include "physics/systems/UpdateOrientationSystem.h"
#include "utils/Profiler.h"

Scene1::Scene1() :
    world(WorldConfig{kMaxEntities, kWorldArenaBytes}),
    systems(world),
    flyCameraController(MATH::Vec3(0.0f, 1.0f, 6.0f), MATH::Vec3(0.0f, 0.0f, 0.0f))
{
}

bool Scene1::OnCreate(ResourceLoader& resourceLoader)
{
    std::cout << "lighting: " << kSchemes[activeScheme].name << ". Keys 1 to " << kSchemes.size() << " switch\n";
    PROFILE_SCOPE_ECS(world, "OnCreate");

    const TextureHandle marioTexture = resourceLoader.CreateTexture("assets/textures/mario_main.png");
    const TextureHandle skullTexture = resourceLoader.
        CreateTexture("assets/textures/skull_texture.jpg");

    marioMesh = resourceLoader.CreateMesh("assets/meshes/Mario.obj");
    marioMaterial = resourceLoader.CreateMaterial({
        .albedo = marioTexture, .shininess = 32.0f, .specularStrength = 0.5f
    });
    cubeMesh = resourceLoader.CreateCube();
    cubeMaterial = resourceLoader.CreateMaterial({
        .albedo = skullTexture, .shininess = 4.0f, .specularStrength = 0.1f
    });

    /* Mario stands in the middle and turns half a radian per second, a full turn in about 12.6 seconds */
    world.CreateEntity(
        Transform{.position = MATH::Vec3()},
        Renderable{marioMesh, marioMaterial},
        PHYSICS::AngularVelocity{MATH::Vec3(0.f, 0.5f, 0.f)}
    );


    /* Two cubes, one on each side */
    constexpr float cubeX[] = {-2.5f, 2.5f};
    constexpr auto up = MATH::Vec3(0.0f, 1.0f, 0.0f);
    for (const float x : cubeX)
    {
        world.CreateEntity(
            Transform{
                .position = MATH::Vec3(x, 0.0f, 0.0f),
                .scale = 1.0f,
                .orientation = x > 0.0f
                                   ? MATH::QMath::angleAxisRotation(45.0f, up)
                                   : MATH::QMath::angleAxisRotation(30.0f, MATH::Vec3(0.0f, 0.0f, 1.0f)) *
                                   MATH::QMath::angleAxisRotation(45.0f, up)
            },
            PHYSICS::BoxCollider{MATH::Vec3(0.5f, 0.5f, 0.5f)},
            Renderable{cubeMesh, cubeMaterial}
        );
    }

    /* In each phase, systems run in the order they are added */
    systems.Add(Phase::Fixed, std::make_unique<UpdateOrientationSystem>(world));
    systems.Add(Phase::Extract, std::make_unique<DrawRenderablesSystem>(world));
    systems.Add(Phase::Extract, std::make_unique<DrawCollidersSystem>(world));
    return true;
}

void Scene1::OnDestroy()
{
    /* Folds the final frame into the run totals, which Main prints. */
    world.ResetFrameStats();
}

void Scene1::Update(const float deltaTime, const InputState& input)
{
    world.ResetFrameStats();
    flyCameraController.Update(deltaTime);
    camera.view = flyCameraController.View();
}

void Scene1::HandleEvents(const SDL_Event& sdlEvent)
{
    if (sdlEvent.type == SDL_EVENT_KEY_DOWN)
    {
        /* SDLK_1 to SDLK_9 are consecutive, so the key's distance from SDLK_1 is the scheme's position */
        const SDL_Keycode key = sdlEvent.key.key;
        if (key >= SDLK_1 && key < SDLK_1 + kSchemes.size())
        {
            activeScheme = static_cast<uint32_t>(key - SDLK_1);
            std::cout << "lighting: " << kSchemes[activeScheme].name << "\n";
        }
    }

    flyCameraController.HandleEvent(sdlEvent);
}
