#include "scenes/Scene0.h"

#include <iostream>
#include <SDL3/SDL.h>

#include "ecs/Archetype.h"
#include "game/Components.h"
#include "utils/Profiler.h"

Scene0::Scene0() : movers(world, COMPONENTS::MaskOf<Position, Velocity>())
{
}

void Scene0::HandleEvents(const SDL_Event& sdlEvent)
{
    if (sdlEvent.type == SDL_EVENT_KEY_DOWN)
    {
        if (sdlEvent.key.key == SDLK_W)
        {
            std::cout << "W pressed" << std::endl;
        }
    }
}

void Scene0::MakeDrawList(DrawList& drawList)
{
    // drawList.clear();
}

bool Scene0::OnCreate()
{
    {
        PROFILE_SCOPE_ECS(world, "OnCreate");
        entities.reserve(kEntityCount);
        for (int i = 0; i < kEntityCount; ++i)
        {
            const Entity entity = world.CreateEntity();
            world.AddComponent<Position>(entity, {static_cast<float>(i), 0.0f, 0.0f});
            world.AddComponent<Velocity>(entity, {1.0f, 0.5f, 0.0f});
            entities.push_back(entity);
        }
    }
    return true;
}

void Scene0::OnDestroy()
{
    /* Folds the final frame into the run totals, which Main prints. */
    world.ResetFrameStats();
}

void Scene0::Update(float deltaTime)
{
    world.ResetFrameStats();
    ++frameNumber;

    {
        PROFILE_SCOPE("Movement");
        for (Archetype* archetype : movers.Matches())
        {
            Position* positions = archetype->Column<Position>();
            const Velocity* velocities = archetype->Column<Velocity>();
            const uint32_t rows = archetype->Count();
            for (uint32_t row = 0; row < rows; ++row)
            {
                positions[row].x += velocities[row].x * deltaTime;
                positions[row].y += velocities[row].y * deltaTime;
                positions[row].z += velocities[row].z * deltaTime;
            }
        }
    }

    const bool damageFrame = frameNumber % kDamageEvery == 0;
    if (damageFrame)
    {
        PROFILE_SCOPE("Damage");
        hitOn = !hitOn;
        for (int i = 0; i < kDamageCount; ++i)
        {
            if (hitOn) commandBuffer.AddComponent<Hit>(entities[i]);
            else commandBuffer.RemoveComponent<Hit>(entities[i]);
        }
    }
    {
        PROFILE_SCOPE_ECS(world, "Playback");
        commandBuffer.Playback(world);
    }
}
