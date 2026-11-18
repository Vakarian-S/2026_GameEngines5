#include "scenes/Scene1.h"
#include <iostream>
#include <SDL3/SDL.h>

#include "MMath.h"
#include "core/Components.h"
#include "core/ResourceLoader.h"
#include "core/systems/DrawRenderablesSystem.h"
#include "utils/Profiler.h"

namespace
{
    /* The floor's top face. The room and Eirene's plinth start here */
    constexpr float kFloorTop = -1.25f;
    /* Eirene's mesh is scaled by this on every axis */
    constexpr float kEireneScale = 0.0001f;
    /* The bottom of Eirene's plinth in her own mesh units */
    constexpr float kEireneBottom = -16281.044f;
    /* The column mesh is 55.86 tall in its own units. This scale makes it 5.55 tall, so its top sinks 0.05 into the beam above */
    constexpr float kColumnScale = 0.0994f;
    /* The bottom of the column in its own mesh units */
    constexpr float kColumnBottom = -27.9295f;

    /**
     * Makes one box entity that fills the space between two corners
     * @param low the corner with the smallest x, y and z
     * @param high the corner with the largest x, y and z
     */
    void CreateBox(World& world, const MeshHandle cube, const MaterialHandle material,
                   const MATH::Vec3& low, const MATH::Vec3& high)
    {
        world.CreateEntity(
            Transform{.position = (low + high) * 0.5f, .scale = high - low},
            Renderable{cube, material}
        );
    }

    /**
     * Makes a wall from four boxes around a window: one below it, one above it and one on each side.
     * The window goes through the wall's full thickness, so only its other two axes matter
     * @param wallLow, wallHigh the corners of the whole wall
     * @param windowLow, windowHigh the corners of the opening
     */
    void CreateWallWithWindow(World& world, const MeshHandle cube, const MaterialHandle material,
                              const MATH::Vec3& wallLow, const MATH::Vec3& wallHigh,
                              const MATH::Vec3& windowLow, const MATH::Vec3& windowHigh)
    {
        CreateBox(world, cube, material, wallLow, MATH::Vec3(wallHigh.x, windowLow.y, wallHigh.z));
        CreateBox(world, cube, material, MATH::Vec3(wallLow.x, windowHigh.y, wallLow.z), wallHigh);

        /* The side pieces split the wall along its long axis */
        const bool runsAlongX = wallHigh.x - wallLow.x > wallHigh.z - wallLow.z;
        if (runsAlongX)
        {
            CreateBox(world, cube, material, MATH::Vec3(wallLow.x, windowLow.y, wallLow.z),
                      MATH::Vec3(windowLow.x, windowHigh.y, wallHigh.z));
            CreateBox(world, cube, material, MATH::Vec3(windowHigh.x, windowLow.y, wallLow.z),
                      MATH::Vec3(wallHigh.x, windowHigh.y, wallHigh.z));
        }
        else
        {
            CreateBox(world, cube, material, MATH::Vec3(wallLow.x, windowLow.y, wallLow.z),
                      MATH::Vec3(wallHigh.x, windowHigh.y, windowLow.z));
            CreateBox(world, cube, material, MATH::Vec3(wallLow.x, windowLow.y, windowHigh.z),
                      MATH::Vec3(wallHigh.x, windowHigh.y, wallHigh.z));
        }
    }
}

Scene1::Scene1() :
    world(WorldConfig{kMaxEntities, kWorldArenaBytes}),
    systems(world),
    flyCameraController(MATH::Vec3(0.0f, 1.0f, 9.0f), MATH::Vec3(0.0f, 0.8f, 0.0f))
{
}

bool Scene1::OnCreate(ResourceLoader& resourceLoader)
{
    std::cout << "lighting: " << kSchemes[activeScheme].name << ". Keys 1 to " << kSchemes.size() << " switch\n";
    PROFILE_SCOPE_ECS(world, "OnCreate");

    /* Skyboxes */
    for (size_t i = 0; i < kSchemes.size(); ++i)
    {
        if (!kSchemes[i].skybox) continue;
        const CubeMapHandle handle = resourceLoader.CreateCubemap(kSchemes[i].skybox);
        skyboxSchemes[i] = handle;
    }

    const TextureHandle floorTexture = resourceLoader.CreateTexture("assets/textures/floor_grey.png");

    EireneMesh = resourceLoader.CreateMesh("assets/meshes/eirene.obj");
    columnMesh = resourceLoader.CreateMesh("assets/meshes/ionic_column.obj");
    cubeMesh = resourceLoader.CreateCube();
    columnMaterial = resourceLoader.CreateMaterial({
        .albedo = resourceLoader.CreateTexture("assets/textures/ionic_column_albedo.jpg"),
        .shininess = 16.0f, .specularStrength = 0.15f
    });
    statueMaterial = resourceLoader.CreateMaterial({
        .albedo = floorTexture, .shininess = 16.0f, .specularStrength = 0.2f
    });
    floorMaterial = resourceLoader.CreateMaterial({
        .albedo = floorTexture, .shininess = 4.0f, .specularStrength = 0.05f
    });

    /* Eirene in the middle of the room */
    world.CreateEntity(
        Transform{
            .position = MATH::Vec3(0.0f, kFloorTop - kEireneBottom * kEireneScale, 0.0f),
            .scale = MATH::Vec3(kEireneScale, kEireneScale, kEireneScale),
            .orientation = MATH::QMath::angleAxisRotation(90.0f, MATH::Vec3(0.0f, 1.0f, 0.0f))
        },
        Renderable{EireneMesh, statueMaterial}
    );

    /*
     * A ruin on a raised base, like a diorama. Only the back wall still stands. The sides are rows of
     * columns, so the side lights pass between them and stripe the floor with column shadows
     */
    constexpr float baseHalfWidth = 6.0f;
    constexpr float baseBottom = kFloorTop - 0.75f;
    constexpr float wallHeight = 5.5f;
    constexpr float wallTop = kFloorTop + wallHeight;

    /* The base: 12 by 12 and 0.75 thick */
    CreateBox(world, cubeMesh, floorMaterial, MATH::Vec3(-baseHalfWidth, baseBottom, -baseHalfWidth),
              MATH::Vec3(baseHalfWidth, kFloorTop, baseHalfWidth));

    /* Two steps down from the front edge, each 0.25 lower and 0.4 deep */
    for (int step = 1; step <= 2; ++step)
    {
        const float front = baseHalfWidth + 0.4f * static_cast<float>(step);
        CreateBox(world, cubeMesh, floorMaterial, MATH::Vec3(-1.5f, baseBottom, front - 0.4f),
                  MATH::Vec3(1.5f, kFloorTop - 0.25f * static_cast<float>(step), front));
    }

    /*
     * The back wall: 10 wide, 0.5 thick and 5.5 tall, with a window 3 wide and 3.5 tall starting 1 above
     * the floor. A cornice 0.5 tall and a little wider than the wall sits on top
     */
    constexpr float wallFront = -5.0f;
    constexpr float wallBack = wallFront - 0.5f;
    CreateWallWithWindow(world, cubeMesh, floorMaterial,
                         MATH::Vec3(-5.0f, kFloorTop, wallBack), MATH::Vec3(5.0f, wallTop, wallFront),
                         MATH::Vec3(-1.5f, kFloorTop + 1.0f, wallBack), MATH::Vec3(1.5f, kFloorTop + 4.5f, wallFront));
    CreateBox(world, cubeMesh, floorMaterial, MATH::Vec3(-5.5f, wallTop, wallBack - 0.25f),
              MATH::Vec3(5.5f, wallTop + 0.5f, wallFront + 0.25f));

    /*
     * Three columns down each side. The column is about 1 wide, so x = 4.4 keeps it on the base.
     * Its top sinks 0.05 above wallTop, into the beam over the back two
     */
    constexpr float columnX = 4.4f;
    for (const float x : {-columnX, columnX})
    {
        for (const float z : {-4.0f, -0.5f, 3.0f})
        {
            world.CreateEntity(
                Transform{
                    .position = MATH::Vec3(x, kFloorTop - kColumnBottom * kColumnScale, z),
                    .scale = MATH::Vec3(kColumnScale, kColumnScale, kColumnScale)
                },
                Renderable{columnMesh, columnMaterial}
            );
        }

        /* The beam that still joins the back two columns to the wall. The front column stands alone */
        const float beamCenterX = x < 0.0f ? -4.4f : 4.4f;
        CreateBox(world, cubeMesh, floorMaterial, MATH::Vec3(beamCenterX - 0.6f, wallTop, wallFront),
                  MATH::Vec3(beamCenterX + 0.6f, wallTop + 0.5f, 0.1f));
    }

    /*
     * Two fallen columns lying on the floor. They are smaller than the standing ones, like broken drums.
     * Turning 90 degrees about z lays the column along x, then the yaw turns it on the floor.
     * The mesh is 9.55 wide on x, so on its side it rests at half that, times the scale, above the floor
     */
    struct FallenColumn
    {
        MATH::Vec3 place;
        float yaw;
    };
    constexpr float fallenScale = 0.06f;
    constexpr float fallenRest = 4.776f * fallenScale;
    for (const FallenColumn& fallen : {
             FallenColumn{MATH::Vec3(-2.4f, 0.0f, 1.3f), 20.0f},
             FallenColumn{MATH::Vec3(1.9f, 0.0f, 2.2f), -35.0f}
         })
    {
        world.CreateEntity(
            Transform{
                .position = MATH::Vec3(fallen.place.x, kFloorTop + fallenRest, fallen.place.z),
                .scale = MATH::Vec3(fallenScale, fallenScale, fallenScale),
                .orientation = MATH::QMath::angleAxisRotation(fallen.yaw, MATH::Vec3(0.0f, 1.0f, 0.0f)) *
                MATH::QMath::angleAxisRotation(90.0f, MATH::Vec3(0.0f, 0.0f, 1.0f))
            },
            Renderable{columnMesh, columnMaterial}
        );
    }

    /* Rubble: loose stone blocks on the floor, each turned a little */
    struct RubbleBlock
    {
        MATH::Vec3 place;
        MATH::Vec3 size;
        float yaw;
    };
    for (const RubbleBlock& block : {
             RubbleBlock{MATH::Vec3(1.8f, 0.0f, -3.2f), MATH::Vec3(0.6f, 0.4f, 0.5f), 15.0f},
             RubbleBlock{MATH::Vec3(-1.6f, 0.0f, -3.5f), MATH::Vec3(0.8f, 0.3f, 0.5f), -20.0f},
             RubbleBlock{MATH::Vec3(3.0f, 0.0f, 0.8f), MATH::Vec3(0.4f, 0.4f, 0.4f), 40.0f},
             RubbleBlock{MATH::Vec3(-3.2f, 0.0f, 4.0f), MATH::Vec3(0.5f, 0.35f, 0.7f), 10.0f}
         })
    {
        world.CreateEntity(
            Transform{
                .position = MATH::Vec3(block.place.x, kFloorTop + block.size.y * 0.5f, block.place.z),
                .scale = block.size,
                .orientation = MATH::QMath::angleAxisRotation(block.yaw, MATH::Vec3(0.0f, 1.0f, 0.0f))
            },
            Renderable{cubeMesh, floorMaterial}
        );
    }

    /* In each phase, systems run in the order they are added */
    systems.Add(Phase::Extract, std::make_unique<DrawRenderablesSystem>(world));
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
