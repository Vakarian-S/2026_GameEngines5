#include "scenes/BossFightScene.h"

#include <iostream>

#include "MMath.h"
#include "core/AssetLoader.h"
#include "core/Components.h"
#include "core/systems/DrawRenderablesSystem.h"
#include "game/Components.h"
#include "game/systems/FigureEightPathSystem.h"
#include "game/systems/ClampToArenaSystem.h"
#include "game/systems/PlayerCommandSystem.h"
#include "game/systems/PlayerMoveSystem.h"
#include "physics/Components.h"
#include "physics/systems/UpdatePositionSystem.h"

namespace
{
    /** Most entities alive at once */
    constexpr uint32_t kMaxEntities = 64;
    /** Bytes in the world's slab. Each archetype reserves kMaxEntities rows*/
    constexpr std::size_t kWorldArenaBytes = 64 * 1024;
    /** Starting Camera eye for the scene, lives on top of the scene for a top down view */
    constexpr MATH::Vec3 kCameraEye = MATH::Vec3(0.f, 18.f, 12.f);
    /** Starting target of the camera, points at the origin of the scene */
    constexpr MATH::Vec3 kCameraTarget = MATH::Vec3(0.0f, 0.0f, 0.0f);
    /** Speed of the player character */
    constexpr float kPlayerSpeed = 6.0f;
    /** Half Width of the stage arena across the X axis*/
    constexpr float kArenaHalfWidth = 10.f;
    /** Half Depth of the stage arena across the Z axis*/
    constexpr float kArenaHalfDepth = 7.f;
    /** Where the boss's figure-eight path crosses itself */
    constexpr MATH::Vec3 kBossPathCenter = MATH::Vec3(0.f, 0.f, -3.f);
    /** How many ticks it takes for the boss to do a full loop */
    constexpr std::uint64_t kBossPeriodTicks = 960;
    /** Half Width of the Boss movement pattern*/
    constexpr float kBossHalfWidth = 6.f;
    /** Half Depth of the Boss movement pattern*/
    constexpr float kBossHalfDepth = 2.f;
}

BossFightScene::BossFightScene() :
    world(WorldConfig{kMaxEntities, kWorldArenaBytes}),
    systems(world)
{
    camera.view = MATH::MMath::lookAt(kCameraEye, kCameraTarget, MATH::Vec3(0.0f, 1.0f, 0.0f));
    camera.farPlane = 100.f;
}

void BossFightScene::Update(float deltaTime, const InputState& input)
{
}

bool BossFightScene::OnCreate(AssetLoader& assetLoader)
{
    std::cout << "BossFightScene::OnCreate" << std::endl;
    /* ======== Player ======== */
    const TextureHandle playerTexture = assetLoader.CreateTexture("assets/textures/mario_main.png");
    const MeshHandle playerMesh = assetLoader.CreateMesh("assets/meshes/Mario.obj");
    const MaterialHandle playerMaterial = assetLoader.CreateMaterial({
        .albedo = playerTexture, .shininess = 32.0f, .specularStrength = 0.5f
    });

    world.CreateEntity(
        Transform{.position = MATH::Vec3(0.f, 0.f, 5.f)},
        PHYSICS::Velocity{},
        Renderable{playerMesh, playerMaterial},
        PlayerCommand{},
        LocalPlayer{}
    );

    /* ======== Boss ======== */
    const TextureHandle bossTexture = assetLoader.CreateTexture("assets/textures/triceratops_palette.png");
    const MeshHandle bossMesh = assetLoader.CreateMesh("assets/meshes/Triceratops.obj");
    const MaterialHandle bossMaterial = assetLoader.CreateMaterial({
        .albedo = bossTexture, .shininess = 32.0f, .specularStrength = 0.5f
    });

    world.CreateEntity(
        Transform{.position = kBossPathCenter, .scale = MATH::Vec3(3.0f, 3.0f, 3.0f)},
        Renderable{bossMesh, bossMaterial},
        FigureEightPath{kBossPathCenter, kBossPeriodTicks, kBossHalfWidth, kBossHalfDepth},
        Boss{}
    );

    systems.Add(Phase::Fixed, std::make_unique<FigureEightPathSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<PlayerMoveSystem>(world, kPlayerSpeed));
    systems.Add(Phase::Fixed, std::make_unique<UpdatePositionSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<ClampToArenaSystem>(world, kArenaHalfWidth, kArenaHalfDepth));
    systems.Add(Phase::Update, std::make_unique<PlayerCommandSystem>(world));
    systems.Add(Phase::Extract, std::make_unique<DrawRenderablesSystem>(world));
    return true;
}

void BossFightScene::OnDestroy()
{
    world.ResetFrameStats();
}

void BossFightScene::HandleEvents(const SDL_Event& sdlEvent)
{
}
