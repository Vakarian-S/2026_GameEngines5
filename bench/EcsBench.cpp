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

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "ecs/Archetype.h"
#include "ecs/CommandBuffer.h"
#include "ecs/Query.h"
#include "ecs/World.h"
#include "utils/Profiler.h"

namespace
{
    /** Where a bench entity sits, in metres. Its own type, so the bench load stays 12 bytes when the engine's components change */
    struct Position
    {
        float x, y, z;
    };

    /** How far a bench entity moves each second, in metres */
    struct Velocity
    {
        float x, y, z;
    };

    /** Marks a bench entity that was hit. Carries nothing, so it is a tag */
    struct Hit
    {
    };

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
    /** Slab bytes per entity. This load peaks at about 49 with chunks of 4 to 64 KB, measured as arena high water at 100,000. */
    constexpr std::size_t kArenaBytesPerEntity = 128;
    /** Slab bytes on top of that, for the archetype objects and their lists */
    constexpr std::size_t kArenaBaseBytes = 64 * 1024;

    /**
     * Fills a world with entities carrying Position and Velocity.
     * @param world world to fill
     * @param count how many entities to make
     */
    void Spawn(World& world, const int count)
    {
        for (int i = 0; i < count; ++i)
        {
            world.CreateEntity(Position{static_cast<float>(i), 0.0f, 0.0f}, Velocity{1.0f, 0.5f, 0.25f});
        }
    }

    /**
     * Walks every matching row and adds velocity to position. The system this design is built for.
     * @param movers query for Position and Velocity
     */
    void Move(Query& movers)
    {
        movers.ForEachChunk([](ChunkView chunk)
        {
            Position* positions = chunk.Column<Position>();
            const Velocity* velocities = chunk.Column<Velocity>();
            const uint32_t rows = chunk.Count();

            for (uint32_t row = 0; row < rows; ++row)
            {
                positions[row].x += velocities[row].x * kFixedDelta;
                positions[row].y += velocities[row].y * kFixedDelta;
                positions[row].z += velocities[row].z * kFixedDelta;
            }
        });
    }

    /**
     * Calls visit with the entity in every stride-th row of each matching archetype. Rows count across
     * chunks, so the rows picked are the same whatever the chunk size.
     * @param query archetypes to walk
     * @param stride visit one row in every this many
     * @param visit called with each picked row's entity
     */
    template <typename Visit>
    void ForEveryStrideRow(Query& query, const uint32_t stride, Visit&& visit)
    {
        const Archetype* current = nullptr;
        uint32_t firstRow = 0;

        query.ForEachChunk([&](const ChunkView chunk)
        {
            /* A new archetype starts its row count again, as the loop over whole archetypes did. */
            if (chunk.Owner() != current)
            {
                current = chunk.Owner();
                firstRow = 0;
            }

            const Entity* owners = chunk.Entities();
            for (uint32_t local = 0; local < chunk.Count(); ++local)
            {
                if ((firstRow + local) % stride == 0) visit(owners[local]);
            }
            firstRow += chunk.Count();
        });
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

        ForEveryStrideRow(movers, kCullStride, [&](const Entity owner)
        {
            buffer.DestroyEntity(owner);
            ++killed;
        });

        for (uint32_t i = 0; i < killed; ++i)
        {
            buffer.CreateEntity(Position{0.0f, 0.0f, 0.0f}, Velocity{1.0f, 0.5f, 0.25f});
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
        ForEveryStrideRow(untagged, kTagStride, [&](const Entity owner)
        {
            buffer.AddComponent<Hit>(owner);
        });

        ForEveryStrideRow(tagged, 1, [&](const Entity owner)
        {
            buffer.RemoveComponent<Hit>(owner);
        });
    }

    /**
     * Runs one scale end to end and prints what it cost.
     * @param count how many entities to start with
     * @param useArena true for one slab sized for count, false for the heap
     * @param chunkBytes bytes in each chunk of the world
     */
    void RunScale(const int count, const bool useArena, const uint32_t chunkBytes)
    {
        WorldConfig config;
        config.maxEntities = static_cast<uint32_t>(count);
        config.arenaBytes = useArena ? kArenaBaseBytes + kArenaBytesPerEntity * static_cast<std::size_t>(count) : 0;
        config.chunkBytes = chunkBytes;

        World world(config);
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

        std::cout << "\n==== " << count << " entities, " << kFrames << " frames, "
            << (useArena ? "arena" : "heap") << ", " << chunkBytes / 1024 << " KB chunks ====\n";
        Profiler::Report();

        std::cout << "  totals    created " << totals.entitiesCreated
            << ", destroyed " << totals.entitiesDestroyed
            << ", migrations " << totals.migrations
            << ", bytes " << totals.bytesCopied << "\n";
        std::cout << "  chunks    taken " << totals.chunksTaken
            << ", returned " << totals.chunksReturned
            << ", " << (totals.chunksTaken - totals.chunksReturned) << " in use, "
            << snapshot.chunksFree << " free\n";
        std::cout << "  world     " << snapshot.liveEntities << " live, "
            << snapshot.archetypeCount << " archetypes ("
            << snapshot.emptyArchetypes << " empty), "
            << snapshot.slotTableSize << " slots\n";

        if (useArena)
        {
            const WorldMemoryStats memory = world.MemoryStats();
            std::cout << "  arena     " << memory.arena.capacity << " bytes, "
                << memory.arena.highWater << " high water, "
                << memory.resource.fromArena << " allocations, "
                << memory.resource.fromHeap << " fell back to the heap\n";
        }
    }
}

int main()
{
    std::cout << "EcsBench\n";

    /* The chunk sizes the plan measures. 16 KB is the World's default and what Unity and Unreal Mass use. */
    constexpr uint32_t kChunkSizes[] = {4 * 1024, 16 * 1024, 64 * 1024};

    for (const int count : {1000, 10000, 100000})
    {
        RunScale(count, false, 16 * 1024);
        for (const uint32_t chunkBytes : kChunkSizes)
        {
            RunScale(count, true, chunkBytes);
        }
    }

    return 0;
}
