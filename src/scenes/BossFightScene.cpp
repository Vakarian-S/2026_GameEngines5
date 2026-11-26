#include "scenes/BossFightScene.h"

#include <iostream>
#include <SDL3/SDL_scancode.h>

#include "MMath.h"
#include "core/AssetLoader.h"
#include "core/Components.h"
#include "core/InputState.h"
#include "core/NetworkSettings.h"
#include "core/systems/DrawRenderablesSystem.h"
#include "core/systems/LifespanSystem.h"
#include "game/Components.h"
#include "game/systems/AcceptClientsSystem.h"
#include "game/systems/BossDeathSystem.h"
#include "game/systems/FigureEightPathSystem.h"
#include "game/systems/ClampToArenaSystem.h"
#include "game/systems/ClientReceiveSystem.h"
#include "game/systems/PlayerCommandSystem.h"
#include "game/systems/PlayerMoveSystem.h"
#include "game/systems/ProjectileHitSystem.h"
#include "game/systems/ServerReceiveSystem.h"
#include "game/systems/ShootSystem.h"
#include "net/ClientConnection.h"
#include "net/ClientTable.h"
#include "net/ConnectionEvents.h"
#include "net/ServerSocket.h"
#include "physics/Components.h"
#include "physics/systems/DrawCollidersSystem.h"
#include "physics/systems/UpdatePositionSystem.h"

namespace
{
    /** Most entities alive at once */
    constexpr uint32_t kMaxEntities = 128;
    /** Bytes in the world's slab. Each archetype takes at least one 16 KB chunk.*/
    constexpr std::size_t kWorldArenaBytes = 256 * 1024;
    /** Starting Camera eye for the scene, high above and behind the arena so the whole floor fits on screen */
    constexpr MATH::Vec3 kCameraEye = MATH::Vec3(0.f, 24.f, 17.f);
    /** Starting target of the camera, points at the origin of the scene */
    constexpr MATH::Vec3 kCameraTarget = MATH::Vec3(0.0f, 0.0f, 0.0f);
    /** Speed of the player character */
    constexpr float kPlayerSpeed = 6.0f;
    /** The bottom of the player's mesh in its own units, so the player's feet touch the floor */
    constexpr float kPlayerMeshBottom = -1.281565f;
    /** Half Width of the stage arena across the X axis*/
    constexpr float kArenaHalfWidth = 14.f;
    /** Half Depth of the stage arena across the Z axis*/
    constexpr float kArenaHalfDepth = 10.f;
    /** Where the boss's figure-eight path crosses itself */
    constexpr MATH::Vec3 kBossPathCenter = MATH::Vec3(0.f, 0.f, -3.f);
    /** How many ticks it takes for the boss to do a full loop */
    constexpr std::uint64_t kBossPeriodTicks = 960;
    /** Half Width of the boss movement pattern*/
    constexpr float kBossHalfWidth = 6.f;
    /** Half Depth of the boss movement pattern*/
    constexpr float kBossHalfDepth = 2.f;
    /** Radius of the boss's sphere collider at scale 1. Above the 0.5 default, so smaller projectiles still hit as often */
    constexpr float kBossColliderRadius = 0.62f;
    /** Speed of a projectile in metres per second */
    constexpr float kProjectileSpeed = 15.f;
    /** Size of a projectile on every axis. It shrinks the projectile's mesh and its collider together */
    constexpr float kProjectileScale = 0.3f;
    /** Ticks a projectile lives before it despawns. 90 ticks is 1.5 s */
    constexpr std::uint64_t kProjectileLifetimeTicks = 90;
    /** Ticks between two shots of the player's gun. 12 ticks is 5 shots a second */
    constexpr std::uint64_t kGunCooldownTicks = 12;
    /** Health Points the boss has */
    constexpr std::int32_t kBossHealth = 20;
    /** Damage each projectile deals to Health Components */
    constexpr std::int32_t kProjectileDamage = 1;

    /** Height of the floor cube's centre */
    constexpr float kFloorCenterY = -1.5f;
    /** Thickness of the floor cube */
    constexpr float kFloorThickness = 0.1f;
    /** The floor's top face. The player, the walls and everything on them stand on it */
    constexpr float kFloorTop = kFloorCenterY + kFloorThickness * 0.5f;

    /** Height of the low walls around the arena */
    constexpr float kWallHeight = 1.0f;
    /** Thickness of the low walls around the arena */
    constexpr float kWallThickness = 0.5f;
    /** The top face of the arena walls. The columns and torches stand on it */
    constexpr float kWallTop = kFloorTop + kWallHeight;

    /** The column mesh is 55.86 tall in its own units. This scale makes it about 3.35 tall */
    constexpr float kColumnScale = 0.06f;
    /** The bottom of the column in its own mesh units */
    constexpr float kColumnBottom = -27.9295f;
    /** Columns on the back wall and on the front wall, from corner to corner */
    constexpr int kColumnsAlongWidth = 5;
    /** Columns on each side wall, spaced between the corners */
    constexpr int kColumnsAlongDepth = 3;

    /** Size of a torch cube on every axis */
    constexpr float kTorchSize = 0.4f;
    /** Distance of each torch from the middle of the back wall. kLights in the header puts a torch light at the same x */
    constexpr float kTorchX = 6.0f;
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
    if (input.Pressed(SDL_SCANCODE_C))
    {
        systems.SetEnabled(drawColliders, !systems.IsEnabled(drawColliders));
    }
}

bool BossFightScene::OnCreate(AssetLoader& assetLoader)
{
    std::cout << "BossFightScene::OnCreate" << std::endl;

    /* ======== Sky ======== */
    sky = assetLoader.CreateCubemap("assets/skyboxes/night");

    /* ======== Floor and walls ======== */
    const TextureHandle floorTexture = assetLoader.CreateTexture("assets/textures/floor_grey.png");
    const MeshHandle cubeMesh = assetLoader.CreateCube();
    const MaterialHandle floorMaterial = assetLoader.CreateMaterial({
        .albedo = floorTexture, .shininess = 4.0f, .specularStrength = 0.1f
    });
    floorRenderable = {cubeMesh, floorMaterial};
    const MaterialHandle wallMaterial = assetLoader.CreateMaterial({
        .albedo = floorTexture, .shininess = 8.0f, .specularStrength = 0.1f
    });
    wallRenderable = {cubeMesh, wallMaterial};

    /* ======== Columns ======== */
    const MeshHandle columnMesh = assetLoader.CreateMesh("assets/meshes/ionic_column.obj");
    const MaterialHandle columnMaterial = assetLoader.CreateMaterial({
        .albedo = assetLoader.CreateTexture("assets/textures/ionic_column_albedo.jpg"),
        .shininess = 16.0f, .specularStrength = 0.15f
    });
    columnRenderable = {columnMesh, columnMaterial};

    /* ======== Torches ======== */
    const MaterialHandle torchMaterial = assetLoader.CreateMaterial({
        .albedo = assetLoader.CreateTexture("assets/textures/mario_main.png"),
        .shininess = 4.0f, .specularStrength = 0.1f
    });
    torchRenderable = {cubeMesh, torchMaterial};

    /* ======== Projectile ======== */
    const TextureHandle projectileTexture = assetLoader.CreateTexture("assets/textures/floor_grey.png");
    const MaterialHandle projectileMaterial = assetLoader.CreateMaterial({
        .albedo = projectileTexture, .shininess = 4.0f, .specularStrength = 0.1f
    });
    projectileRenderable = {cubeMesh, projectileMaterial, false};

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
    drawColliders = systems.Add(Phase::Extract, std::make_unique<DrawCollidersSystem>(world));
    systems.SetEnabled(drawColliders, false);

    /* ======== Network ======== */
    const NetworkSettings& network = world.GetResources().Get<NetworkSettings>();
    if (network.role == NetworkRole::Server)
    {
        world.GetResources().Insert<NET::ServerSocket>(network.port);
        world.GetResources().Insert<NET::ClientTable>();
        world.GetResources().Insert<NET::ConnectionEventQueue>();
        systems.Add(Phase::Fixed, std::make_unique<AcceptClientsSystem>(world));
        systems.Add(Phase::Fixed, std::make_unique<ServerReceiveSystem>(world));
    }
    else if (network.role == NetworkRole::Client)
    {
        const std::string address = network.host + ":" + std::to_string(network.port);
        std::cout << "[Client] connecting to " << address << "\n";
        world.GetResources().Insert<NET::ClientConnection>(address);
        world.GetResources().Insert<NET::ConnectionEventQueue>();
        systems.Add(Phase::Update, std::make_unique<ClientReceiveSystem>(world));
    }

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
    SpawnFloor();
    SpawnWalls();
    SpawnColumns();
    SpawnTorches();
    SpawnPlayer();
    SpawnBoss();
}

void BossFightScene::SpawnFloor()
{
    world.CreateEntity(
        TransformComponent{
            .position = MATH::Vec3(0.f, kFloorCenterY, 0.f),
            .scale = MATH::Vec3(2 * kArenaHalfWidth, kFloorThickness, 2 * kArenaHalfDepth)
        },
        floorRenderable
    );
}

void BossFightScene::SpawnWalls()
{
    const float wallCenterY = kFloorTop + kWallHeight * 0.5f;
    /* The back and front walls run past the side walls by one thickness, so they close the corners */
    const float longWallLength = 2 * kArenaHalfWidth + 2 * kWallThickness;
    const float wallOffset = kWallThickness * 0.5f;

    for (const float side : {-1.f, 1.f})
    {
        world.CreateEntity(
            TransformComponent{
                .position = MATH::Vec3(0.f, wallCenterY, side * (kArenaHalfDepth + wallOffset)),
                .scale = MATH::Vec3(longWallLength, kWallHeight, kWallThickness)
            },
            wallRenderable
        );
        world.CreateEntity(
            TransformComponent{
                .position = MATH::Vec3(side * (kArenaHalfWidth + wallOffset), wallCenterY, 0.f),
                .scale = MATH::Vec3(kWallThickness, kWallHeight, 2 * kArenaHalfDepth)
            },
            wallRenderable
        );
    }
}

void BossFightScene::SpawnColumns()
{
    const float columnCenterY = kWallTop - kColumnBottom * kColumnScale;
    const MATH::Vec3 columnScale(kColumnScale, kColumnScale, kColumnScale);
    const float wallOffset = kWallThickness * 0.5f;

    /* Back and front walls: the first and last column stand on the corners */
    const float widthSpacing = 2 * kArenaHalfWidth / (kColumnsAlongWidth - 1);
    for (int i = 0; i < kColumnsAlongWidth; ++i)
    {
        const float x = -kArenaHalfWidth + i * widthSpacing;
        for (const float side : {-1.f, 1.f})
        {
            world.CreateEntity(
                TransformComponent{
                    .position = MATH::Vec3(x, columnCenterY, side * (kArenaHalfDepth + wallOffset)),
                    .scale = columnScale
                },
                columnRenderable
            );
        }
    }

    /* Side walls: the corners already have columns, so these sit evenly between them */
    const float depthSpacing = 2 * kArenaHalfDepth / (kColumnsAlongDepth + 1);
    for (int i = 1; i <= kColumnsAlongDepth; ++i)
    {
        const float z = -kArenaHalfDepth + i * depthSpacing;
        for (const float side : {-1.f, 1.f})
        {
            world.CreateEntity(
                TransformComponent{
                    .position = MATH::Vec3(side * (kArenaHalfWidth + wallOffset), columnCenterY, z),
                    .scale = columnScale
                },
                columnRenderable
            );
        }
    }
}

void BossFightScene::SpawnTorches()
{
    const float backWallZ = -kArenaHalfDepth - kWallThickness * 0.5f;
    for (const float x : {-kTorchX, kTorchX})
    {
        world.CreateEntity(
            TransformComponent{
                .position = MATH::Vec3(x, kWallTop + kTorchSize * 0.5f, backWallZ),
                .scale = MATH::Vec3(kTorchSize, kTorchSize, kTorchSize)
            },
            torchRenderable
        );
    }
}

void BossFightScene::SpawnPlayer()
{
    world.CreateEntity(
        TransformComponent{.position = MATH::Vec3(0.f, kFloorTop - kPlayerMeshBottom, 5.f)},
        PHYSICS::VelocityComponent{},
        playerRenderable,
        PlayerCommandComponent{},
        LocalPlayerTag{},
        GunComponent{
            .projectileSpeed = kProjectileSpeed,
            .lifetimeTicks = kProjectileLifetimeTicks,
            .cooldownTicks = kGunCooldownTicks,
            .projectileDamage = kProjectileDamage,
            .projectileScale = kProjectileScale,
            .projectileRenderable = projectileRenderable,
        }
    );
}

void BossFightScene::SpawnBoss()
{
    world.CreateEntity(
        TransformComponent{.position = kBossPathCenter, .scale = MATH::Vec3(3.0f, 3.0f, 3.0f)},
        bossRenderable,
        FigureEightPathComponent{kBossPathCenter, kBossPeriodTicks, kBossHalfWidth, kBossHalfDepth},
        BossTag{},
        PHYSICS::SphereColliderComponent{.radius = kBossColliderRadius},
        HealthComponent{.points = kBossHealth}
    );
}