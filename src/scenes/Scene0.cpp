#include "scenes/Scene0.h"

#include <iostream>
#include <SDL3/SDL.h>

#include "MMath.h"
#include "ecs/Archetype.h"
#include "game/Components.h"
#include "utils/Profiler.h"

namespace
{
    /**
     * Brings a coordinate that left the box back in through the opposite wall.
     * @param value one coordinate of a position
     * @param halfWidth half the width of the box
     * @return the coordinate, inside the box
     */
    float WrapIntoBox(const float value, const float halfWidth)
    {
        if (value > halfWidth) return value - 2.0f * halfWidth;
        if (value < -halfWidth) return value + 2.0f * halfWidth;
        return value;
    }
}

Scene0::Scene0(const MeshHandle cubeMesh_, const MaterialHandle firstMaterial_, const MaterialHandle secondMaterial_) :
    world(WorldConfig{kEntityCount, kWorldArenaBytes}),
    movers(world, COMPONENTS::MaskOf<Position, Velocity>()),
    cubeMesh(cubeMesh_),
    materials{firstMaterial_, secondMaterial_}
{
    /* The grid is centred on the origin and spans about 20 metres, so 40 metres back shows all of it */
    camera.view = MATH::MMath::lookAt(MATH::Vec3(0.0f, 0.0f, 40.0f), MATH::Vec3(0.0f, 0.0f, 0.0f),
                                      MATH::Vec3(0.0f, 1.0f, 0.0f));
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
    uint32_t total = 0;
    for (const Archetype* archetype : movers.Matches()) total += archetype->Count();

    /* Reserved once, so the list takes one block from the caller's memory. */
    drawList.reserve(total);

    for (Archetype* archetype : movers.Matches())
    {
        const Position* positions = archetype->Column<Position>();
        const uint32_t rows = archetype->Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            /* Even rows take the first material and odd rows the second, so every item binds a new set: the worst case */
            drawList.push_back({
                cubeMesh, materials[row % 2],
                MATH::MMath::translate(positions[row].x, positions[row].y, positions[row].z)
            });
        }
    }
}

bool Scene0::OnCreate()
{
    {
        PROFILE_SCOPE_ECS(world, "OnCreate");
        entities.reserve(kEntityCount);
        for (uint32_t i = 0; i < kEntityCount; ++i)
        {
            /* The cell this entity sits in, counted from one corner of the grid */
            const uint32_t column = i % kGridSide;
            const uint32_t row = (i / kGridSide) % kGridSide;
            const uint32_t layer = i / (kGridSide * kGridSide);
            /* Moves the grid's centre onto the origin */
            constexpr float centre = 0.5f * static_cast<float>(kGridSide - 1);

            const Entity entity = world.CreateEntity();
            world.AddComponent<Position>(entity, {
                                             (static_cast<float>(column) - centre) * kGridSpacing,
                                             (static_cast<float>(row) - centre) * kGridSpacing,
                                             (static_cast<float>(layer) - centre) * kGridSpacing
                                         });
            /* Each entity gets a different speed between -1.5 and 1.5 m/s on each axis, so the grid breaks up as it moves */
            world.AddComponent<Velocity>(entity, {
                                             0.5f * static_cast<float>(static_cast<int>(i % 7) - 3),
                                             0.5f * static_cast<float>(static_cast<int>(i % 5) - 2),
                                             0.5f * static_cast<float>(static_cast<int>(i % 3) - 1)
                                         });
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
                positions[row].x = WrapIntoBox(positions[row].x + velocities[row].x * deltaTime, kBoxHalfWidth);
                positions[row].y = WrapIntoBox(positions[row].y + velocities[row].y * deltaTime, kBoxHalfWidth);
                positions[row].z = WrapIntoBox(positions[row].z + velocities[row].z * deltaTime, kBoxHalfWidth);
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
