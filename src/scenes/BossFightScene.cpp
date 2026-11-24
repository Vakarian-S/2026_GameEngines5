#include "scenes/BossFightScene.h"

#include <iostream>
#include <SDL3/SDL_scancode.h>

#include "MMath.h"
#include "core/AssetLoader.h"
#include "core/Components.h"
#include "core/InputState.h"
#include "core/systems/DrawRenderablesSystem.h"
#include "core/systems/LifespanSystem.h"
#include "game/Components.h"
#include "game/systems/BossDeathSystem.h"
#include "game/systems/FigureEightPathSystem.h"
#include "game/systems/ClampToArenaSystem.h"
#include "game/systems/PlayerCommandSystem.h"
#include "game/systems/PlayerMoveSystem.h"
#include "game/systems/ProjectileHitSystem.h"
#include "game/systems/ShootSystem.h"
#include "physics/Components.h"
#include "physics/systems/DrawCollidersSystem.h"
#include "physics/systems/UpdatePositionSystem.h"

namespace
{
    /** Most entities alive at once */
    constexpr uint32_t kMaxEntities = 64;
    /** Bytes in the world's slab. Each archetype takes at least one 16 KB chunk.*/
    constexpr std::size_t kWorldArenaBytes = 256 * 1024;
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
    /** Speed of a projectile in metres per second */
    constexpr float kProjectileSpeed = 15.f;
    /** Ticks a projectile lives before it despawns. 90 ticks is 1.5 s */
    constexpr std::uint64_t kProjectileLifetimeTicks = 90;
    /** Ticks between two shots of the player's gun. 12 ticks is 5 shots a second */
    constexpr std::uint64_t kGunCooldownTicks = 12;
    /** Health Points the boss has */
    constexpr std::int32_t kBossHealth = 20;
    /** Damage each projectile deals to Health Components */
    constexpr std::int32_t kProjectileDamage = 1;
    /** Thickness of the floor cube */
    constexpr float kFloorThickness = 0.1f;
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
    if (input.Pressed(SDL_SCANCODE_R))
    {
        world.Clear();
        SpawnEntities();
    }
}

bool BossFightScene::OnCreate(AssetLoader& assetLoader)
{
    std::cout << "BossFightScene::OnCreate" << std::endl;

    /* ======== Floor ======== */
    const TextureHandle floorTexture = assetLoader.CreateTexture("assets/textures/floor_grey.png");
    const MeshHandle floorMesh = assetLoader.CreateCube();
    const MaterialHandle floorMaterial = assetLoader.CreateMaterial({
        .albedo = floorTexture, .shininess = 4.0f, .specularStrength = 0.1f
    });
    floorRenderable = {floorMesh, floorMaterial};

    /* ======== Projectile ======== */
    const TextureHandle projectileTexture = assetLoader.CreateTexture("assets/textures/floor_grey.png");
    const MeshHandle projectileMesh = assetLoader.CreateCube();
    const MaterialHandle projectileMaterial = assetLoader.CreateMaterial({
        .albedo = projectileTexture, .shininess = 4.0f, .specularStrength = 0.1f
    });
    projectileRenderable = {projectileMesh, projectileMaterial};

    /* ======== Player ======== */
    const TextureHandle playerTexture = assetLoader.CreateTexture("assets/textures/mario_main.png");
    const MeshHandle playerMesh = assetLoader.CreateMesh("assets/meshes/Mario.obj");
    const MaterialHandle playerMaterial = assetLoader.CreateMaterial({
        .albedo = playerTexture, .shininess = 32.0f, .specularStrength = 0.5f
    });
    playerRenderable = {playerMesh, playerMaterial};

    /* ======== Boss ======== */
    const TextureHandle bossTexture = assetLoader.CreateTexture("assets/textures/triceratops_palette.png");
    const MeshHandle bossMesh = assetLoader.CreateMesh("assets/meshes/Triceratops.obj");
    const MaterialHandle bossMaterial = assetLoader.CreateMaterial({
        .albedo = bossTexture, .shininess = 32.0f, .specularStrength = 0.5f
    });
    bossRenderable = {bossMesh, bossMaterial};

    SpawnEntities();

    /* ======== Systems ======== */
    systems.Add(Phase::Fixed, std::make_unique<FigureEightPathSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<PlayerMoveSystem>(world, kPlayerSpeed));
    systems.Add(Phase::Fixed, std::make_unique<UpdatePositionSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<ClampToArenaSystem>(world, kArenaHalfWidth, kArenaHalfDepth));
    systems.Add(Phase::Fixed, std::make_unique<ShootSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<LifespanSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<ProjectileHitSystem>(world));
    systems.Add(Phase::Fixed, std::make_unique<BossDeathSystem>(world));
    systems.Add(Phase::Update, std::make_unique<PlayerCommandSystem>(world));
    systems.Add(Phase::Extract, std::make_unique<DrawRenderablesSystem>(world));
    systems.Add(Phase::Extract, std::make_unique<DrawCollidersSystem>(world));

    return true;
}

void BossFightScene::OnDestroy()
{
    world.ResetFrameStats();
}

void BossFightScene::HandleEvents(const SDL_Event& sdlEvent)
{
}

void BossFightScene::SpawnEntities()
{
    /* ======== Floor ======== */
    world.CreateEntity(
        Transform{
            .position = MATH::Vec3(0.f, -1.5f, 0.f),
            .scale = MATH::Vec3(2 * kArenaHalfWidth, kFloorThickness, 2 * kArenaHalfDepth)
        },
        floorRenderable
        );
    /* ======== Player ======== */
    world.CreateEntity(
        Transform{.position = MATH::Vec3(0.f, 0.f, 5.f)},
        PHYSICS::Velocity{},
        playerRenderable,
        PlayerCommand{},
        LocalPlayer{},
        GunComponent{
            .projectileSpeed = kProjectileSpeed,
            .lifetimeTicks = kProjectileLifetimeTicks,
            .cooldownTicks = kGunCooldownTicks,
            .projectileDamage = kProjectileDamage,
            .projectileRenderable = projectileRenderable,
        }
    );

    /* ======== Boss ======== */
    world.CreateEntity(
        Transform{.position = kBossPathCenter, .scale = MATH::Vec3(3.0f, 3.0f, 3.0f)},
        bossRenderable,
        FigureEightPath{kBossPathCenter, kBossPeriodTicks, kBossHalfWidth, kBossHalfDepth},
        BossComponent{},
        PHYSICS::SphereCollider{},
        HealthComponent{.points = kBossHealth}
    );
}
