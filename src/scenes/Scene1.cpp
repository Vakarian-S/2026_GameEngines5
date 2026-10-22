#include "scenes/Scene1.h"
#include <iostream>
#include <SDL3/SDL.h>

#include "ConstantsConversions.h"
#include "MMath.h"
#include "core/ResourceLoader.h"
#include "ecs/Archetype.h"
#include "game/Components.h"
#include "utils/Profiler.h"

Scene1::Scene1() :
    world(WorldConfig{kMaxEntities, kWorldArenaBytes}),
    spinners(world, COMPONENTS::MaskOf<Rotation, Spin>()),
    drawables(world, COMPONENTS::MaskOf<Position, Rotation, Renderable>()),
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
    const Entity mario = world.CreateEntity();
    world.AddComponent<Position>(mario, {0.0f, 0.0f, 0.0f});
    world.AddComponent<Rotation>(mario, {0.0f});
    world.AddComponent<Renderable>(mario, {marioMesh, marioMaterial});
    world.AddComponent<Spin>(mario, {0.5f});

    /* Two cubes, one on each side. They have no Spin, so the spinners query never sees them */
    constexpr float cubeX[] = {-2.5f, 2.5f};
    for (const float x : cubeX)
    {
        const Entity cube = world.CreateEntity();
        world.AddComponent<Position>(cube, {x, 0.0f, 0.0f});
        /* Turned 45 degrees so two faces show and the light falls on them unevenly */
        world.AddComponent<Rotation>(cube, {0.25f * FLOAT_PI});
        world.AddComponent<Renderable>(cube, {cubeMesh, cubeMaterial});
    }
    return true;
}

void Scene1::OnDestroy()
{
    /* Folds the final frame into the run totals, which Main prints. */
    world.ResetFrameStats();
}

void Scene1::Update(const float deltaTime)
{
    world.ResetFrameStats();

    PROFILE_SCOPE("Spin");
    /* Reads Spin, writes Rotation */
    for (Archetype* archetype : spinners.Matches())
    {
        Rotation* rotations = archetype->Column<Rotation>();
        const Spin* spins = archetype->Column<Spin>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            rotations[row].yawRadians += spins[row].radiansPerSecond * deltaTime;
            /* Kept inside one turn, so the float does not lose precision as the angle grows */
            if (rotations[row].yawRadians > 2.0f * FLOAT_PI) rotations[row].yawRadians -= 2.0f * FLOAT_PI;
        }
    }
    flyCameraController.Update(deltaTime);
    camera.view = flyCameraController.View();
}

void Scene1::MakeDrawList(DrawList& drawList)
{
    uint32_t total = 0;
    for (const Archetype* archetype : drawables.Matches()) total += archetype->Count();

    /* Reserved once, so the list takes one block from the caller's memory. */
    drawList.reserve(total);

    /* Reads Position, Rotation and Renderable, writes nothing in the world */
    for (Archetype* archetype : drawables.Matches())
    {
        const Position* positions = archetype->Column<Position>();
        const Rotation* rotations = archetype->Column<Rotation>();
        const Renderable* renderables = archetype->Column<Renderable>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            /* The matrix turns the mesh about its own centre first, then moves it into place. MMath::rotate takes degrees */
            const MATH::Matrix4 transform =
                MATH::MMath::translate(positions[row].x, positions[row].y, positions[row].z) *
                MATH::MMath::rotate(rotations[row].yawRadians * RADIANS_TO_DEGREES, 0.0f, 1.0f, 0.0f);
            drawList.push_back({renderables[row].meshHandle, renderables[row].materialHandle, transform});
        }
    }
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
