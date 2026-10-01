/*
 * Shows what the arena changes, built as its own program.
 *
 * The Memory Monitor counts every call to new. This program does the same work twice, once on
 * the heap and once from an arena, and prints both counts side by side.
 *
 * Nothing here is a test and nothing here touches SDL or Vulkan, so it builds on Win32 as well
 * as x64.
 */

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory_resource>
#include <vector>

#include "ecs/World.h"
#include "game/Components.h"
#include "utils/Arena.h"
#include "utils/ArenaResource.h"
#include "utils/MemoryMonitor.h"

namespace
{
    /** Entities each world spawns */
    constexpr int kEntities = 1000;
    /** Bytes in the arena world's slab */
    constexpr std::size_t kWorldArenaBytes = 256 * 1024;
    /** Frames the scratch demo runs */
    constexpr int kFrames = 5;
    /** Positions each frame collects into its scratch list */
    constexpr int kScratchItems = 1000;
    /** Bytes in the frame arena */
    constexpr std::size_t kFrameArenaBytes = 64 * 1024;

    /** Calls to new, and the bytes they asked for, between two readings of the monitor. */
    struct HeapCost
    {
        std::uint64_t calls = 0;
        std::uint64_t bytes = 0;
    };

    /**
     * @param before a reading taken before the work
     * @return what the heap served since that reading
     */
    HeapCost Since(const MemoryStats& before)
    {
        const MemoryStats now = MemoryMonitor::GetStats();
        return {now.allocationCount - before.allocationCount, now.bytesAllocated - before.bytesAllocated};
    }

    /**
     * Builds a world, spawns kEntities into it, and measures what the heap served.
     * @param config how the world takes its memory
     * @param memory filled with the world's arena counters
     * @return calls to new and bytes, from the world's constructor to the last spawn
     */
    HeapCost BuildWorld(const WorldConfig& config, WorldMemoryStats& memory)
    {
        const MemoryStats before = MemoryMonitor::GetStats();

        World world(config);
        for (int i = 0; i < kEntities; ++i)
        {
            const Entity entity = world.CreateEntity();
            world.AddComponent<Position>(entity, {static_cast<float>(i), 0.0f, 0.0f});
            world.AddComponent<Velocity>(entity, {1.0f, 0.5f, 0.0f});
        }

        memory = world.MemoryStats();
        return Since(before);
    }

    /**
     * Runs kFrames frames that each fill a scratch list on the heap.
     * @return calls to new and bytes across every frame
     */
    HeapCost HeapFrames()
    {
        const MemoryStats before = MemoryMonitor::GetStats();

        for (int frame = 0; frame < kFrames; ++frame)
        {
            std::vector<Position> scratch;
            for (int i = 0; i < kScratchItems; ++i) scratch.push_back({static_cast<float>(i), 0.0f, 0.0f});
        }

        return Since(before);
    }

    /**
     * Runs the same frames with the scratch list in a frame arena, reset once per frame.
     * @param frameArena the arena to fill and reset
     * @param frameMemory the resource over that arena
     * @return calls to new and bytes across every frame
     */
    HeapCost ArenaFrames(Arena& frameArena, ArenaResource& frameMemory)
    {
        const MemoryStats before = MemoryMonitor::GetStats();

        for (int frame = 0; frame < kFrames; ++frame)
        {
            {
                std::pmr::vector<Position> scratch(&frameMemory);
                for (int i = 0; i < kScratchItems; ++i) scratch.push_back({static_cast<float>(i), 0.0f, 0.0f});
            }

            /* The list is gone before the reset, so nothing points into the slab afterwards. */
            frameArena.Reset();
        }

        return Since(before);
    }
}

int main()
{
    std::cout << "ArenaDemo\n";

    /* ---- The permanent arena: one world, built twice ---- */

    WorldConfig heapConfig;
    heapConfig.arenaBytes = 0;

    WorldConfig arenaConfig;
    arenaConfig.maxEntities = kEntities;
    arenaConfig.arenaBytes = kWorldArenaBytes;

    WorldMemoryStats unused;
    WorldMemoryStats memory;
    const HeapCost onHeap = BuildWorld(heapConfig, unused);
    const HeapCost onArena = BuildWorld(arenaConfig, memory);

    std::cout << "\n-- a world of " << kEntities << " entities, Position and Velocity --\n"
        << "  heap    " << onHeap.calls << " calls to new, " << onHeap.bytes << " bytes\n"
        << "  arena   " << onArena.calls << " calls to new, " << onArena.bytes << " bytes\n"
        << "  inside the arena\n"
        << "    slab          " << memory.arena.capacity << " bytes\n"
        << "    used          " << memory.arena.used << " bytes\n"
        << "    padding       " << memory.arena.padding << " bytes\n"
        << "    allocations   " << memory.resource.fromArena << "\n"
        << "    fell to heap  " << memory.resource.fromHeap << "\n";

    /* ---- The frame arena: scratch memory, reset every frame ---- */

    Arena frameArena(kFrameArenaBytes);
    ArenaResource frameMemory(frameArena);

    const HeapCost heapFrames = HeapFrames();
    const HeapCost arenaFrames = ArenaFrames(frameArena, frameMemory);
    const ArenaStats frameStats = frameArena.Stats();

    std::cout << "\n-- " << kFrames << " frames, each filling a list of " << kScratchItems << " positions --\n"
        << "  heap    " << heapFrames.calls << " calls to new, " << heapFrames.bytes << " bytes\n"
        << "  arena   " << arenaFrames.calls << " calls to new, " << arenaFrames.bytes << " bytes\n"
        << "  inside the frame arena\n"
        << "    slab          " << frameStats.capacity << " bytes, taken once before the first frame\n"
        << "    high water    " << frameStats.highWater << " bytes\n"
        << "    resets        " << frameStats.resets << "\n"
        << "    fell to heap  " << frameMemory.Stats().fromHeap << "\n";

    return 0;
}
