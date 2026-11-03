#include "scenes/PhysicsScene.h"

#include "core/Components.h"
#include "core/ResourceLoader.h"
#include "core/systems/DrawRenderablesSystem.h"
#include "physics/Components.h"
#include "physics/systems/DrawCollidersSystem.h"
#include "utils/Profiler.h"
#include "core/InputState.h"
#include "physics/RigidBody.h"
#include "physics/systems/ApplyForcesSystem.h"
#include "physics/systems/DrawGrabLineSystem.h"
#include "physics/systems/PickSystem.h"
#include "physics/systems/DrawPickRaySystem.h"
#include "physics/systems/MouseConstraintSystem.h"
#include "physics/systems/UpdateOrientationSystem.h"
#include "physics/systems/UpdatePositionSystem.h"

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
    constexpr PHYSICS::BoxCollider cubeCollider{};
    world.AddComponent<PHYSICS::BoxCollider>(cube, cubeCollider);
    world.AddComponent<Renderable>(cube, {cubeMesh, cubeMaterial});
    world.AddComponent<PHYSICS::Velocity>(cube, {});
    world.AddComponent<PHYSICS::AngularVelocity>(cube, {});
    world.AddComponent<PHYSICS::MassProperties>(cube, {
                                                    .mass = 1.0f,
                                                    .inertiaTensor = PHYSICS::InertiaTensor(
                                                        cubeCollider, 1.0f, 1.0f)
                                                });

    const Entity cube2 = world.CreateEntity();
    world.AddComponent<Transform>(cube2, {
                                      .position = MATH::Vec3(
                                          6.f, 0.5f, 0.f),
                                      .scale = 1.0f, .orientation = MATH::QMath::angleAxisRotation(
                                          30.0f, MATH::Vec3(0.0f, 0.0f, 1.0f))
                                  });
    world.AddComponent<PHYSICS::BoxCollider>(cube2, cubeCollider);
    world.AddComponent<Renderable>(cube2, {cubeMesh, cubeMaterial});
    world.AddComponent<PHYSICS::Velocity>(cube2, {});
    world.AddComponent<PHYSICS::AngularVelocity>(cube2, {MATH::Vec3(0.f, 0.5f, 0.f)});
    world.AddComponent<PHYSICS::MassProperties>(cube2, {
                                                    .mass = 1.0f,
                                                    .inertiaTensor = PHYSICS::InertiaTensor(
                                                        cubeCollider, 1.0f, 1.0f)
                                                });

    const Entity mario1 = world.CreateEntity();
    world.AddComponent<Transform>(mario1, {
                                      .position = MATH::Vec3(
                                          -6.f, 1.3f, 0.f
                                      )
                                  });
    constexpr PHYSICS::SphereCollider sphereCollider{.radius = 1.3f};
    world.AddComponent<PHYSICS::SphereCollider>(mario1, sphereCollider);
    world.AddComponent<Renderable>(mario1, {marioMesh, marioMaterial});
    world.AddComponent<PHYSICS::Velocity>(mario1, {});
    world.AddComponent<PHYSICS::AngularVelocity>(mario1, {});
    world.AddComponent<PHYSICS::MassProperties>(mario1, {
                                                    .mass = 1.f,
                                                    .inertiaTensor = PHYSICS::InertiaTensor(sphereCollider, 1.f, 1.f)
                                                });


    const Entity mario2 = world.CreateEntity();
    world.AddComponent<Transform>(mario2, {
                                      .position = MATH::Vec3(
                                          -2.f, 1.3f, 0.f
                                      )
                                  });
    constexpr PHYSICS::CapsuleCollider capsuleCollider{0.6f, 0.7f};
    world.AddComponent<PHYSICS::CapsuleCollider>(mario2, capsuleCollider);
    world.AddComponent<Renderable>(mario2, {marioMesh, marioMaterial});
    world.AddComponent<PHYSICS::Velocity>(mario2, {});
    world.AddComponent<PHYSICS::AngularVelocity>(mario2, {});
    world.AddComponent<PHYSICS::MassProperties>(mario2, {
                                                    .mass = 1.f,
                                                    .inertiaTensor = PHYSICS::InertiaTensor(capsuleCollider, 1.f, 1.f)
                                                });
    const Entity mario3 = world.CreateEntity();
    world.AddComponent<Transform>(mario3, {
                                      .position = MATH::Vec3(
                                          2.f, 1.3f, 0.f
                                      )
                                  });
    constexpr PHYSICS::CylinderCollider cylinderCollider{0.7f, 1.3f};
    world.AddComponent<PHYSICS::CylinderCollider>(mario3, cylinderCollider);
    world.AddComponent<Renderable>(mario3, {marioMesh, marioMaterial});
    world.AddComponent<PHYSICS::Velocity>(mario3, {});
    world.AddComponent<PHYSICS::AngularVelocity>(mario3, {});
    world.AddComponent<PHYSICS::MassProperties>(mario3, {
                                                    .mass = 1.f,
                                                    .inertiaTensor = PHYSICS::InertiaTensor(cylinderCollider, 1.f, 1.f)
                                                });


    const Entity mario4 = world.CreateEntity();
    world.AddComponent<Transform>(mario4, {
                                      .position = MATH::Vec3(
                                          11.f, 2.6f, 0.f
                                      ),
                                      .scale = 2.0f
                                  });
    world.AddComponent<PHYSICS::SphereCollider>(mario4, sphereCollider);
    world.AddComponent<Renderable>(mario4, {marioMesh, marioMaterial});
    world.AddComponent<PHYSICS::Velocity>(mario4, {});
    world.AddComponent<PHYSICS::AngularVelocity>(mario4, {});
    world.AddComponent<PHYSICS::MassProperties>(mario4, {
                                                    .mass = 1.f,
                                                    .inertiaTensor = PHYSICS::InertiaTensor(sphereCollider, 1.f, 2.f)
                                                });

    const Entity pickRay = world.CreateEntity();
    world.AddComponent<PHYSICS::PickRay>(pickRay, {});

    systems.Add(Phase::Update, std::make_unique<PickSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<ApplyForcesSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<MouseConstraintSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<UpdatePositionSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<UpdateOrientationSystem>(world));
    drawRenderables = systems.Add(Phase::Extract, std::make_unique<DrawRenderablesSystem>(world));
    drawColliders = systems.Add(Phase::Extract, std::make_unique<DrawCollidersSystem>(world));
    drawPickRay = systems.Add(Phase::Extract, std::make_unique<DrawPickRaySystem>(world));
    drawGrabLines = systems.Add(Phase::Extract, std::make_unique<DrawGrabLineSystem>(world));

    return true;
}

void PhysicsScene::OnDestroy()
{
    /* Folds the final frame into the run totals, which Main prints. */
    world.ResetFrameStats();
}

void PhysicsScene::Update(const float deltaTime, const InputState& input)
{
    world.ResetFrameStats();
    flyCameraController.Update(deltaTime);
    camera.view = flyCameraController.View();
    if (input.Pressed(SDL_SCANCODE_C))
    {
        systems.SetEnabled(drawColliders, !systems.IsEnabled(drawColliders));
        systems.SetEnabled(drawPickRay, !systems.IsEnabled(drawPickRay));
        systems.SetEnabled(drawGrabLines, !systems.IsEnabled(drawGrabLines));
    }
    if (input.Pressed(SDL_SCANCODE_M)) systems.SetEnabled(drawRenderables, !systems.IsEnabled(drawRenderables));
}

void PhysicsScene::HandleEvents(const SDL_Event& sdlEvent)
{
    flyCameraController.HandleEvent(sdlEvent);
}
