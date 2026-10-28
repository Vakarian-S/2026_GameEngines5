#include "scenes/PhysicsScene.h"

#include "core/Components.h"
#include "core/ResourceLoader.h"
#include "core/systems/DrawRenderablesSystem.h"
#include "game/Components.h"
#include "game/systems/SpinSystem.h"
#include "physics/Components.h"
#include "physics/systems/DrawCollidersSystem.h"
#include "utils/Profiler.h"
#include "core/InputState.h"

PhysicsScene::PhysicsScene() :
    world(WorldConfig{kMaxEntities, kWorldArenaBytes}),
    systems(world),
    flyCameraController(MATH::Vec3(0.f, 3.f, 16.f), MATH::Vec3(0.0f, 1.0f, 0.0f))
{
}

bool PhysicsScene::OnCreate(ResourceLoader& resourceLoader)
{
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

    /* ========== Entities ======== */
    const Entity cube = world.CreateEntity();
    world.AddComponent<Transform>(cube, {
                                      .position = MATH::Vec3(
                                          -10.f, 0.5f, 0.f
                                      )
                                  });
    world.AddComponent<PHYSICS::BoxCollider>(cube, {});
    world.AddComponent<Renderable>(cube, {cubeMesh, cubeMaterial});

    const Entity cube2 = world.CreateEntity();
    world.AddComponent<Transform>(cube2, {
                                      .position = MATH::Vec3(
                                          6.f, 0.5f, 0.f),
                                      .scale = 1.0f, .orientation = MATH::QMath::angleAxisRotation(
                                          30.0f, MATH::Vec3(0.0f, 0.0f, 1.0f))
                                  });
    world.AddComponent<PHYSICS::BoxCollider>(cube2, {});
    world.AddComponent<Spin>(cube2, {0.5f});
    world.AddComponent<Renderable>(cube2, {cubeMesh, cubeMaterial});

    const Entity mario1 = world.CreateEntity();
    world.AddComponent<Transform>(mario1, {
                                      .position = MATH::Vec3(
                                          -6.f, 1.3f, 0.f
                                      )
                                  });
    world.AddComponent<PHYSICS::SphereCollider>(mario1, {1.3f});
    world.AddComponent<Renderable>(mario1, {marioMesh, marioMaterial});


    const Entity mario2 = world.CreateEntity();
    world.AddComponent<Transform>(mario2, {
                                      .position = MATH::Vec3(
                                          -2.f, 1.3f, 0.f
                                      )
                                  });
    world.AddComponent<PHYSICS::CapsuleCollider>(mario2, {0.6f, 0.7f});
    world.AddComponent<Renderable>(mario2, {marioMesh, marioMaterial});


    const Entity mario3 = world.CreateEntity();
    world.AddComponent<Transform>(mario3, {
                                      .position = MATH::Vec3(
                                          2.f, 1.3f, 0.f
                                      )
                                  });
    world.AddComponent<PHYSICS::CylinderCollider>(mario3, {0.7f, 1.3f});
    world.AddComponent<Renderable>(mario3, {marioMesh, marioMaterial});


    const Entity mario4 = world.CreateEntity();
    world.AddComponent<Transform>(mario4, {
                                      .position = MATH::Vec3(
                                          11.f, 2.6f, 0.f
                                      ),
                                      .scale = 2.0f
                                  });
    world.AddComponent<PHYSICS::SphereCollider>(mario4, {1.3f});
    world.AddComponent<Renderable>(mario4, {marioMesh, marioMaterial});

    systems.Add(Phase::Fixed, std::make_unique<SpinSystem>(world));
    drawRenderables = systems.Add(Phase::Extract, std::make_unique<DrawRenderablesSystem>(world));
    drawColliders = systems.Add(Phase::Extract, std::make_unique<DrawCollidersSystem>(world));

    return true;
}

void PhysicsScene::OnDestroy()
{
    /* Folds the final frame into the run totals, which Main prints. */
    world.ResetFrameStats();
}

void PhysicsScene::Update(float deltaTime, const InputState& input)
{
    world.ResetFrameStats();
    flyCameraController.Update(deltaTime);
    camera.view = flyCameraController.View();
    if (input.Pressed(SDL_SCANCODE_C)) systems.SetEnabled(drawColliders, !systems.IsEnabled(drawColliders));
    if (input.Pressed(SDL_SCANCODE_M)) systems.SetEnabled(drawRenderables, !systems.IsEnabled(drawRenderables));
}

void PhysicsScene::HandleEvents(const SDL_Event& sdlEvent)
{
    flyCameraController.HandleEvent(sdlEvent);
}
