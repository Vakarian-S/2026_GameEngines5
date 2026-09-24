#pragma once
#include <vector>

#include "ecs/Entity.h"
#include "ecs/EntitySlot.h"

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
    bool IsEntityAlive(Entity entity) const;

private:
    /** One record per index ever handed out. The index into this vector is the entity's index. */
    std::vector<EntitySlot> entitySlots;
    /** Indexes of dead slots, newest first. */
    std::vector<uint32_t> freeSlots;
};
