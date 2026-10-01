#pragma once
#include <memory_resource>
#include <vector>
#include <cstddef>

#include "Archetype.h"
#include "Component.h"
#include "ecs/Entity.h"
#include "ecs/EntitySlot.h"
#include "FrameStats.h"
#include "utils/Arena.h"
#include "utils/ArenaResource.h"

class Archetype;

/** How much memory a World takes, and from where. */
struct WorldConfig
{
    /** Most entities alive at once. Every archetype reserves this many rows. */
    uint32_t maxEntities = 1024;
    /**
     * Bytes in the world's slab, taken from the OS once. Zero means no arena: every container
     * grows on the heap, as the world did before the arena existed.
     */
    std::size_t arenaBytes = 1024 * 1024;
};

/** What a World's arena holds, and how much of the world missed it. */
struct WorldMemoryStats
{
    ArenaStats arena;
    ArenaResourceStats resource;
};
/**
 * Owns every entity in the game.
 *
 * The directory is two vectors. entitySlots is the record for each index ever handed out,
 * and freeSlots holds the indexes of the dead ones, waiting to be reused. They have to stay
 * consistent with each other, which is why they are private and why this is a class rather
 * than a struct.
 *
 * Nothing outside ever sees an EntitySlot. Callers hold an Entity, which is two numbers.
 */
class World
{
public:
    /**
     * @param config how many entities to make room for, and how large a slab to take
     */
    explicit World(const WorldConfig& config = {});
    ~World();

    /**
     * Makes a live entity and returns a handle to it.
     *
     * Reuses a dead index when one is free, so the slot table grows to the peak number of
     * live entities and then stops. A reused index comes back with a higher version, which
     * is what makes handles to the previous occupant read as dead.
     */
    Entity CreateEntity();

    /**
     * Kills an entity and puts its index back in the pool.
     *
     * Bumping the version here is what invalidates every copy of the handle at once, with no
     * list of who was holding one. Passing an already-dead handle does nothing.
     */
    void DestroyEntity(Entity entity);

    /**
     * Whether this exact entity is still the one living in its slot.
     *
     * Checks two things: that the versions match, and that the slot is not sitting free. The
     * second matters for a slot that was destroyed and not yet reused, where the version
     * alone would still agree.
     *
     * Systems iterating an archetype do not call this. Everything in an archetype is alive by
     * definition. It is for stored handles, such as an AI target from a previous frame.
     */
    bool DoesEntityExist(Entity entity) const;

    /**
     * The entity's component of type T, or nullptr if it is gone or has no T.
     *
     * Not for tags. A tag stores nothing, so there is no data to point at.
     * @tparam T Type of the component to find
     * @param entity Entity that owns said component
     * @return The component itself.
     */
    template <typename T>
    T* GetComponent(const Entity entity)
    {
        static_assert(!std::is_empty_v<T>,
                      "A tag holds no data. Use HasComponent<T>() to test whether an entity has one.");

        if (!DoesEntityExist(entity))
        {
            return nullptr;
        }

        const EntitySlot& slot = entitySlots[entity.index];
        T* column = archetypes[slot.archetype]->Column<T>();
        return column != nullptr ? column + slot.row : nullptr;
    }

    /**
     * Whether the entity has T. Tests the mask, so this works for tags as well as components. 
     * @tparam T Type of component to find
     * @param entity entity to search in
     * @return boolean indicating whether the entity has the component or not
     */
    template <typename T>
    bool HasComponent(Entity entity) const
    {
        if (!DoesEntityExist(entity))
        {
            return false;
        }
        const EntitySlot& slot = entitySlots[entity.index];
        return (archetypes[slot.archetype]->Mask() & COMPONENTS::MaskOf<T>()) != 0;
    }

    /**
     * Adds a component to an entity, migrating it from its current archetype to its new one
     * @tparam T Type of the component to add
     * @param entity Entity to add the component to
     * @param value Value of the created Component.
     */
    template <typename T>
    void AddComponent(const Entity entity, const T& value = T{})
    {
        /* A tag has no column, so there are no bytes to hand over. */
        if constexpr (std::is_empty_v<T>)
        {
            AddComponentById(entity, COMPONENTS::IdOf<T>(), nullptr, 0);
        }
        else
        {
            AddComponentById(entity, COMPONENTS::IdOf<T>(),
                             reinterpret_cast<const std::byte*>(&value), sizeof(T));
        }
    }

    template <typename T>
    void RemoveComponent(const Entity entity)
    {
        RemoveComponentById(entity, COMPONENTS::IdOf<T>());
    }

    /**
     * Where an entity's data sits: which archetype, and which row inside it.
     *
     * Debug and tests only. A row is correct only until the next structural change, so never
     * store what comes back. Gameplay code wants GetComponent.
     *
     * @param entity The entity to look up.
     * @param archetype Filled with the archetype index.
     * @param row Filled with the row index.
     * @return false if the entity is gone, and then both outputs are left alone.
     */
    bool Locate(Entity entity, uint16_t& archetype, uint32_t& row) const;

    /**
     * Returns the current archetype generation, used for query validation
     * @return number of the generation 
     */
    uint32_t ArchetypeGeneration() const { return archetypeGeneration; }

    /** Fills out with every archetype that has all of `all` and none of `none`. */
    void CollectMatching(ComponentMask all, ComponentMask none, std::vector<Archetype*>& out);

    /** @return the counters for this frame */
    const FrameStats& Stats() const { return frameStats; }

    /** @return the counters added up over every frame so far */
    const FrameStats& Totals() const { return runTotals; }

    /** Adds this frame into the run totals, then clears the frame's counters. */
    void ResetFrameStats();

    /**
     * @return what the world holds right now
     */
    WorldStats Snapshot() const;

    /** @return the arena's counters, and how many calls went to the heap instead */
    WorldMemoryStats MemoryStats() const;

    /**
     * Adds a component named by its runtime id, copying size bytes into the new column.
     * For playback and tooling, where no type is available. A tag passes nullptr and 0.
     * @param entity Entity to add the component to
     * @param id Id of the component type
     * @param value Bytes to copy from, or nullptr
     * @param size Bytes to copy, which must match the registered size
     */
    void AddComponentById(Entity entity, ComponentId id, const std::byte* value, uint32_t size);

    /**
    * Removes a component named by its runtime id.
    * @param entity Entity to remove the component from
    * @param id Id of the component type
    */
    void RemoveComponentById(Entity entity, ComponentId id);

private:
    /* Declared first, so they are built before every container below and destroyed after them. */

    /** The one block this world takes from the OS */
    Arena arena;
    /** Where every container below allocates. With no slab behind it, every call goes to the heap. */
    ArenaResource memory;
    /** Rows each archetype reserves, or zero when there is no arena */
    uint32_t reservedRows;

    /** One record per index ever handed out. The index into this vector is the entity's index. */
    std::pmr::vector<EntitySlot> entitySlots;
    /** Indexes of dead slots, newest first. */
    std::pmr::vector<uint32_t> freeSlots;

    /** List of all the archetypes in our world. Each one is built in place inside memory. **/
    std::pmr::vector<Archetype*> archetypes;

    /** Goes up by one each time a new archetype is created. Nothing else changes it. */
    uint32_t archetypeGeneration = 0;

    /** Structural changes in this frame */
    FrameStats frameStats;
    
    /** Structural changes across every frame so far */
    FrameStats runTotals;

    /**
     * Gets the index for the archetype that matches the given mask, if no archetype is found, it adds it.
     * @param mask Mask of the archetype 
     * @return 
     */
    uint16_t ArchetypeIndexFor(ComponentMask mask);

    /**
     * Moves an entity from their current archetype to the archetype of newMask, carrying all the components they both share
     * @param entity Entity to be moved
     * @param newMask Mask of the target archetype.
     */
    void MoveEntity(Entity entity, ComponentMask newMask);
};
