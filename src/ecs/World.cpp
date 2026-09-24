#include "ecs/World.h"


World::World()
{
    entitySlots.reserve(64);
    freeSlots.reserve(64);
}

Entity World::CreateEntity()
{
    uint32_t index;
    
    if (!freeSlots.empty())
    {
        /* Reuse the most recently freed index. Its version was already bumped by Destroy. */
        index = freeSlots.back();
        freeSlots.pop_back();
    }
    else
    {
        /* Grow the table. A new slot starts at version 1, never 0, so a default Entity{} never matches it. */
        index = static_cast<uint32_t>(entitySlots.size());
        entitySlots.push_back({1, kInvalidArchetype, 0});
    }

    entitySlots[index].archetype = 0;
    entitySlots[index].row = 0;

    return {index, entitySlots[index].version};
}

void World::DestroyEntity(const Entity entity)
{
    /* A stale handle is normal, not an error. Something died while someone still held it. */
    if (!IsEntityAlive(entity))
    {
        return;
    }

    /* The version bump is the invalidation. Every copy of this handle is now stale. */
    entitySlots[entity.index].version++;
    entitySlots[entity.index].archetype = kInvalidArchetype;
    freeSlots.push_back(entity.index);
}

bool World::IsEntityAlive(const Entity entity) const
{
    /* A default-constructed or garbage handle must answer false, not read past the end. */
    if (entity.index >= entitySlots.size())
    {
        return false;
    }
    
    return entitySlots[entity.index].version == entity.version &&
        entitySlots[entity.index].archetype != kInvalidArchetype;
}
