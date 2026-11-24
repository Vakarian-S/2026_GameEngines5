#include "scenes/PhysicsScene.h"

#include "core/Components.h"
#include "core/AssetLoader.h"
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

bool PhysicsScene::OnCreate(AssetLoader& assetLoader)
{
    PROFILE_SCOPE_ECS(world, "OnCreate");
    const TextureHandle marioTexture = assetLoader.CreateTexture("assets/textures/mario_main.png");
    const TextureHandle skullTexture = assetLoader.
        CreateTexture("assets/textures/skull_texture.jpg");

    marioMesh = assetLoader.CreateMesh("assets/meshes/Mario.obj");
    marioMaterial = assetLoader.CreateMaterial({
        .albedo = marioTexture, .shininess = 32.0f, .specularStrength = 0.5f
    });
    cubeMesh = assetLoader.CreateCube();
    cubeMaterial = assetLoader.CreateMaterial({
        .albedo = skullTexture, .shininess = 4.0f, .specularStrength = 0.1f
    });
    SpawnShapes();
    
    /* ========== Systems ========== */
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
    if (input.Pressed(SDL_SCANCODE_R))
    {
        world.Clear();
        SpawnShapes();
    }
}

void PhysicsScene::HandleEvents(const SDL_Event& sdlEvent)
{
    flyCameraController.HandleEvent(sdlEvent);
}

void PhysicsScene::SpawnShapes()
{
    constexpr PHYSICS::BoxColliderComponent cubeCollider{};
    world.CreateEntity(
        TransformComponent{
            .position = MATH::Vec3(
                -10.f, 0.5f, 0.f
            )
        },
        cubeCollider,
        RenderableComponent{cubeMesh, cubeMaterial},
        PHYSICS::VelocityComponent{},
        PHYSICS::AngularVelocityComponent{},
        PHYSICS::MassPropertiesComponent{
            .mass = 1.0f,
            .inertiaTensor = PHYSICS::InertiaTensor(
                cubeCollider, 1.0f, MATH::Vec3(1.0f, 1.0f, 1.0f))
        }
    );
    
    world.CreateEntity(
        TransformComponent{
            .position = MATH::Vec3(6.f, 0.5f, 0.f),
            .scale = MATH::Vec3(1.0f, 1.0f, 1.0f),
            .orientation = MATH::QMath::angleAxisRotation(30.0f, MATH::Vec3(0.0f, 0.0f, 1.0f))
        },
        cubeCollider,
        RenderableComponent{cubeMesh, cubeMaterial},
        PHYSICS::VelocityComponent{},
        PHYSICS::AngularVelocityComponent{MATH::Vec3(0.f, 0.5f, 0.f)},
        PHYSICS::MassPropertiesComponent{
            .mass = 1.0f,
            .inertiaTensor = PHYSICS::InertiaTensor(cubeCollider, 1.0f, MATH::Vec3(1.0f, 1.0f, 1.0f))
        }
    );
    
    constexpr PHYSICS::SphereColliderComponent sphereCollider{.radius = 1.3f};
    world.CreateEntity(
        TransformComponent{.position = MATH::Vec3(-6.f, 1.3f, 0.f)},
        sphereCollider,
        RenderableComponent{marioMesh, marioMaterial},
        PHYSICS::VelocityComponent{},
        PHYSICS::AngularVelocityComponent{},
        PHYSICS::MassPropertiesComponent{
            .mass = 1.f,
            .inertiaTensor = PHYSICS::InertiaTensor(sphereCollider, 1.f, MATH::Vec3(1.f, 1.f, 1.f))
        }
    );
    
    constexpr PHYSICS::CapsuleColliderComponent capsuleCollider{0.6f, 0.7f};
    world.CreateEntity(
        TransformComponent{.position = MATH::Vec3(-2.f, 1.3f, 0.f)},
        capsuleCollider,
        RenderableComponent{marioMesh, marioMaterial},
        PHYSICS::VelocityComponent{},
        PHYSICS::AngularVelocityComponent{},
        PHYSICS::MassPropertiesComponent{
            .mass = 1.f,
            .inertiaTensor = PHYSICS::InertiaTensor(capsuleCollider, 1.f, MATH::Vec3(1.f, 1.f, 1.f))
        }
    );
    
    constexpr PHYSICS::CylinderColliderComponent cylinderCollider{0.7f, 1.3f};
    world.CreateEntity(
        TransformComponent{.position = MATH::Vec3(2.f, 1.3f, 0.f)},
        cylinderCollider,
        RenderableComponent{marioMesh, marioMaterial},
        PHYSICS::VelocityComponent{},
        PHYSICS::AngularVelocityComponent{},
        PHYSICS::MassPropertiesComponent{
            .mass = 1.f,
            .inertiaTensor = PHYSICS::InertiaTensor(cylinderCollider, 1.f, MATH::Vec3(1.f, 1.f, 1.f))
        }
    );
    
    world.CreateEntity(
        TransformComponent{.position = MATH::Vec3(11.f, 2.6f, 0.f), .scale = MATH::Vec3(2.0f, 2.0f, 2.0f)},
        sphereCollider,
        RenderableComponent{marioMesh, marioMaterial},
        PHYSICS::VelocityComponent{},
        PHYSICS::AngularVelocityComponent{},
        PHYSICS::MassPropertiesComponent{
            .mass = 1.f,
            .inertiaTensor = PHYSICS::InertiaTensor(sphereCollider, 1.f, MATH::Vec3(2.f, 2.f, 2.f))
        }
    );
    world.CreateEntity(PHYSICS::PickRayComponent{});
}
