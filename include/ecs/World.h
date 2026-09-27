#pragma once
#include <memory>
#include <vector>

#include "Archetype.h"
#include "Component.h"
#include "ecs/Entity.h"
#include "ecs/EntitySlot.h"

class Archetype;
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
    World();

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
        if (!DoesEntityExist(entity)) return;
        const ComponentMask currentArchetypeMask = archetypes[entitySlots[entity.index].archetype]->Mask();
        /* Reminder: the OR operator (|) means that we add the bit of the component to the archetype mask */
        MoveEntity(entity, currentArchetypeMask | COMPONENTS::MaskOf<T>());

        /* A tag has no column. if constexpr stops GetComponent<T> existing at all for one. */
        if constexpr (!std::is_empty_v<T>)
        {
            if (T* stored = GetComponent<T>(entity))
            {
                *stored = value;
            }
        }
    }

    template <typename T>
    void RemoveComponent(const Entity entity)
    {
        if (!DoesEntityExist(entity)) return;
        const ComponentMask currentArchetypeMask = archetypes[entitySlots[entity.index].archetype]->Mask();
        /* & = And, ~ = Invert. AND operator against the inverse means, remove the 1's from the second max in the first mask */
        MoveEntity(entity, currentArchetypeMask & ~COMPONENTS::MaskOf<T>());
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

private:
    /** One record per index ever handed out. The index into this vector is the entity's index. */
    std::vector<EntitySlot> entitySlots;
    /** Indexes of dead slots, newest first. */
    std::vector<uint32_t> freeSlots;

    /** List of all the archetypes in our world **/
    std::vector<std::unique_ptr<Archetype>> archetypes;

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
