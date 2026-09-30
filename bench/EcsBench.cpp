/*
 * A load for the profiler, built as its own program.
 *
 * Main.cpp runs a thousand entities behind a 60 fps wait, which is too small and too idle to
 * measure. This runs flat out at three scales so the per frame numbers come from hundreds of
 * samples instead of one, and so a sampling profiler has something to find.
 *
 * Release only, for numbers worth keeping. Debug is roughly fifteen times slower here.
 *
 * Nothing in it touches SDL or Vulkan, so it builds on Win32 as well as x64.
 */

#include <cstdint>
#include <iostream>
#include <vector>

#include "ecs/Archetype.h"
#include "ecs/CommandBuffer.h"
#include "ecs/Query.h"
#include "ecs/World.h"
#include "game/Components.h"
#include "utils/Profiler.h"

namespace
{
    /** Frames each scale runs for */
    constexpr int kFrames = 1000;
    /** Seconds per frame handed to the movement system, so the numbers do not depend on speed */
    constexpr float kFixedDelta = 1.0f / 60.0f;
    /** Frames between one cull and the next */
    constexpr int kCullEvery = 10;
    /** Frames between one tag pass and the next */
    constexpr int kTagEvery = 5;
    /** One row in every this many dies on a cull frame */
    constexpr uint32_t kCullStride = 20;
    /** One row in every this many gains or loses the tag on a tag frame */
    constexpr uint32_t kTagStride = 10;

    /**
     * Fills a world with entities carrying Position and Velocity.
     * @param world world to fill
     * @param count how many entities to make
     */
    void Spawn(World& world, const int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const Entity entity = world.CreateEntity();
            world.AddComponent<Position>(entity, {static_cast<float>(i), 0.0f, 0.0f});
            world.AddComponent<Velocity>(entity, {1.0f, 0.5f, 0.25f});
        }
    }

    /**
     * Walks every matching row and adds velocity to position. The system this design is built for.
     * @param movers query for Position and Velocity
     */
    void Move(Query& movers)
    {
        for (Archetype* archetype : movers.Matches())
        {
            Position* positions = archetype->Column<Position>();
            const Velocity* velocities = archetype->Column<Velocity>();
            const uint32_t rows = archetype->Count();

            for (uint32_t row = 0; row < rows; ++row)
            {
                positions[row].x += velocities[row].x * kFixedDelta;
                positions[row].y += velocities[row].y * kFixedDelta;
                positions[row].z += velocities[row].z * kFixedDelta;
            }
        }
    }

    /**
     * Queues a destroy for one row in every kCullStride, and a replacement for each one.
     * @param movers query for Position and Velocity
     * @param buffer buffer to record into
     * @return how many replacements were queued
     */
    uint32_t CullAndRespawn(Query& movers, CommandBuffer& buffer)
    {
        uint32_t killed = 0;

        for (Archetype* archetype : movers.Matches())
        {
            const Entity* owners = archetype->EntityColumn();
            const uint32_t rows = archetype->Count();

            for (uint32_t row = 0; row < rows; row += kCullStride)
            {
                buffer.DestroyEntity(owners[row]);
                ++killed;
            }
        }

        for (uint32_t i = 0; i < killed; ++i)
        {
            const Entity replacement = buffer.CreateEntity();
            buffer.AddComponent<Position>(replacement, {0.0f, 0.0f, 0.0f});
            buffer.AddComponent<Velocity>(replacement, {1.0f, 0.5f, 0.25f});
        }

        return killed;
    }

    /**
     * Queues the tag onto untagged rows and off tagged ones, which is what drives archetype churn.
     * @param untagged query for Position and Velocity without Hit
     * @param tagged query for Position, Velocity and Hit
     * @param buffer buffer to record into
     */
    void ToggleTags(Query& untagged, Query& tagged, CommandBuffer& buffer)
    {
        for (Archetype* archetype : untagged.Matches())
        {
            const Entity* owners = archetype->EntityColumn();
            const uint32_t rows = archetype->Count();

            for (uint32_t row = 0; row < rows; row += kTagStride)
            {
                buffer.AddComponent<Hit>(owners[row]);
            }
        }

        for (Archetype* archetype : tagged.Matches())
        {
            const Entity* owners = archetype->EntityColumn();
            const uint32_t rows = archetype->Count();

            for (uint32_t row = 0; row < rows; ++row)
            {
                buffer.RemoveComponent<Hit>(owners[row]);
            }
        }
    }

    /**
     * Runs one scale end to end and prints what it cost.
     * @param count how many entities to start with
     */
    void RunScale(const int count)
    {
        World world;
        CommandBuffer buffer;
        Query movers(world, COMPONENTS::MaskOf<Position, Velocity>());
        Query untagged(world, COMPONENTS::MaskOf<Position, Velocity>(), COMPONENTS::MaskOf<Hit>());
        Query tagged(world, COMPONENTS::MaskOf<Position, Velocity, Hit>());

        Profiler::Reset();

        {
            PROFILE_SCOPE_ECS(world, "Spawn");
            Spawn(world, count);
        }

        for (int frame = 1; frame <= kFrames; ++frame)
        {
            Profiler::BeginFrame();
            world.ResetFrameStats();

            {
                PROFILE_SCOPE("Movement");
                Move(movers);
            }

            if (frame % kCullEvery == 0)
            {
                PROFILE_SCOPE("CullAndRespawn");
                CullAndRespawn(movers, buffer);
            }

            if (frame % kTagEvery == 0)
            {
                PROFILE_SCOPE("ToggleTags");
                ToggleTags(untagged, tagged, buffer);
            }

            {
                PROFILE_SCOPE_ECS(world, "Playback");
                buffer.Playback(world);
            }

            Profiler::EndFrame();
        }

        /* Folds the final frame's counters into the run totals. */
        world.ResetFrameStats();

        const FrameStats& totals = world.Totals();
        const WorldStats snapshot = world.Snapshot();

        std::cout << "\n==== " << count << " entities, " << kFrames << " frames ====\n";
        Profiler::Report();

        std::cout << "  totals    created " << totals.entitiesCreated
            << ", destroyed " << totals.entitiesDestroyed
            << ", migrations " << totals.migrations
            << ", bytes " << totals.bytesCopied << "\n";
        std::cout << "  world     " << snapshot.liveEntities << " live, "
            << snapshot.totalRows << " rows, "
            << snapshot.archetypeCount << " archetypes ("
            << snapshot.emptyArchetypes << " empty), "
            << snapshot.slotTableSize << " slots\n";
    }
}

int main()
{
    std::cout << "EcsBench\n";

    for (const int count : {1000, 10000, 100000})
    {
        RunScale(count);
    }

    return 0;
}
