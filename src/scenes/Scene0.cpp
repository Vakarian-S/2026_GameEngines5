#include "scenes/Scene0.h"

#include <iostream>
#include <SDL3/SDL.h>

#include "MMath.h"
#include "core/Components.h"
#include "core/FlyCameraController.h"
#include "core/ResourceLoader.h"
#include "core/systems/DrawRenderablesSystem.h"
#include "game/Components.h"
#include "game/systems/DamageSystem.h"
#include "physics/Components.h"
#include "physics/systems/MovementSystem.h"
#include "utils/Profiler.h"

Scene0::Scene0() :
    world(WorldConfig{kEntityCount, kWorldArenaBytes}),
    systems(world),
    flyCameraController(MATH::Vec3(0.0f, 0.0f, 40.0f), MATH::Vec3(0.0f, 0.0f, 0.0f))
{
}


void Scene0::HandleEvents(const SDL_Event& sdlEvent)
{
    flyCameraController.HandleEvent(sdlEvent);
}

bool Scene0::OnCreate(ResourceLoader& resourceLoader)
{
    {
        PROFILE_SCOPE_ECS(world, "OnCreate");
        cubeMesh = resourceLoader.CreateCube();
        const TextureHandle mario = resourceLoader.CreateTexture("assets/textures/mario_main.png");
        const TextureHandle skull = resourceLoader.
            CreateTexture("assets/textures/skull_texture.jpg");
        const auto shinyMario = resourceLoader.CreateMaterial({
            .albedo = mario, .shininess = 32.0f, .specularStrength = 0.5f
        });
        const auto matteSkull = resourceLoader.CreateMaterial({
            .albedo = skull, .shininess = 4.0f, .specularStrength = 0.1f
        });
        materials = {matteSkull, shinyMario};
        for (uint32_t i = 0; i < kEntityCount; ++i)
        {
            /* The cell this entity sits in, counted from one corner of the grid */
            const uint32_t column = i % kGridSide;
            const uint32_t row = (i / kGridSide) % kGridSide;
            const uint32_t layer = i / (kGridSide * kGridSide);
            /* Moves the grid's centre onto the origin */
            constexpr float centre = 0.5f * static_cast<float>(kGridSide - 1);

            const Entity entity = world.CreateEntity();
            world.AddComponent<Transform>(entity, {
                                              .position = MATH::Vec3(
                                                  (static_cast<float>(column) - centre) * kGridSpacing,
                                                  (static_cast<float>(row) - centre) * kGridSpacing,
                                                  (static_cast<float>(layer) - centre) * kGridSpacing)
                                          });
            /* Each entity gets a different speed between -1.5 and 1.5 m/s on each axis, so the grid breaks up as it moves */
            world.AddComponent<PHYSICS::Velocity>(entity, {
                                             MATH::Vec3(0.5f * static_cast<float>(static_cast<int>(i % 7) - 3),
                                                        0.5f * static_cast<float>(static_cast<int>(i % 5) - 2),
                                                        0.5f * static_cast<float>(static_cast<int>(i % 3) - 1))
                                         });
            world.AddComponent<Renderable>(entity, {cubeMesh, materials[i % 2]});
            if (i < kDamageCount) world.AddComponent<Damageable>(entity);
        }

        systems.Add(Phase::Fixed, std::make_unique<MovementSystem>(world, kBoxHalfWidth));
        systems.Add(Phase::Update, std::make_unique<DamageSystem>(world, kDamageEvery));
        systems.Add(Phase::Extract, std::make_unique<DrawRenderablesSystem>(world));
    }
    return true;
}

void Scene0::OnDestroy()
{
    /* Folds the final frame into the run totals, which Main prints. */
    world.ResetFrameStats();
}

void Scene0::Update(const float deltaTime)
{
    world.ResetFrameStats();
    flyCameraController.Update(deltaTime);
    camera.view = flyCameraController.View();
}
