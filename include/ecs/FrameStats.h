#pragma once
#include <cstdint>

/* ============================== Switch ============================== */

/* On in Debug and Release. Set to 0 to stop the counting. */
#ifndef GE5_ECS_STATS
#define GE5_ECS_STATS 1
#endif

#if GE5_ECS_STATS
/** Adds to one counter. Takes the struct by name, so the counting site stays readable. */
#define GE5_ECS_COUNT(stats, field, amount) ((stats).field += (amount))
#else
/** Counting off - does nothing */
#define GE5_ECS_COUNT(stats, field, amount) ((void)0)
#endif

/**
 * Structural Changes in one frame. World clears these when a frame starts
 */
struct FrameStats
{
    /**
     * Entities Made with CreateEntity
     */
    uint64_t entitiesCreated = 0;
    /**
     * Entities destroyed with DestroyEntity
     */
    uint64_t entitiesDestroyed = 0;
    /**
     * Components added with AddComponent
     */
    uint64_t componentsAdded = 0;

    /**
     * Componentes removed with RemoveComponent
     */
    uint64_t componentsRemoved = 0;

    /**
     * Entities moved from one archetype to another
     */
    uint64_t migrations = 0;

    /**
     * Bytes copied by the archetypes moved
     */
    uint64_t bytesCopied = 0;

    /**
     * Archetypes created by adding new components to entities
     */
    uint64_t archetypesCreated = 0;
};

/**
 * Counters gained between two readings, so one block of work can be measured on its own.
 * @param before World::Stats() read before the work
 * @param after World::Stats() read after the work
 * @return the counters the work added, field by field
 */
inline FrameStats StatsSince(const FrameStats& before, const FrameStats& after)
{
    FrameStats gained;
    gained.componentsAdded = after.componentsAdded - before.componentsAdded;
    gained.componentsRemoved = after.componentsRemoved - before.componentsRemoved;
    gained.entitiesCreated = after.entitiesCreated - before.entitiesCreated;
    gained.entitiesDestroyed = after.entitiesDestroyed - before.entitiesDestroyed;
    gained.migrations = after.migrations - before.migrations;
    gained.bytesCopied = after.bytesCopied - before.bytesCopied;
    gained.archetypesCreated = after.archetypesCreated - before.archetypesCreated;
    return gained;
}

/**
 * Adds one frame's counters onto a running total.
 * @param totals running total to add to
 * @param frame the frame to add
 */
inline void AddStats(FrameStats& totals, const FrameStats& frame)
{
    totals.entitiesCreated += frame.entitiesCreated;
    totals.entitiesDestroyed += frame.entitiesDestroyed;
    totals.componentsAdded += frame.componentsAdded;
    totals.componentsRemoved += frame.componentsRemoved;
    totals.migrations += frame.migrations;
    totals.bytesCopied += frame.bytesCopied;
    totals.archetypesCreated += frame.archetypesCreated;
}

/**
 * What command buffer's last playback did. Playback fills this in
 */
struct CommandBufferStats
{
    /**
     * Commands the last playback applied
     */
    uint32_t commandsPlayed = 0;

    /**
     * Placeholder Entities the last playback created
     */
    uint32_t placeholdersCreated = 0;

    /**
     * Bytes of component values the last playback carried
     */
    uint32_t payloadBytes = 0;
};

/**
 * Stats for the current World, Built by World:Snapshot
 */
struct WorldStats
{
    /**
     * Amount of Archetypes that exist
     */
    uint32_t archetypeCount = 0;

    /**
     * Archetypes that holds no rows
     */
    uint32_t emptyArchetypes = 0;

    /**
     * Entities currently alive
     */
    uint32_t liveEntities = 0;

    /**
     * Total rows across every archetype
     */
    uint32_t totalRows = 0;

    /**
     * Entity Slots handed out, live and dead
     */
    uint32_t slotTableSize = 0;
};
