#include "core/World.h"

World::World()
{
    entitySlots.reserve(64);
    freeSlots.reserve(64);
}

Entity World::CreateEntity()
{
    uint32_t index;
    /* Free slot available */
    if (!freeSlots.empty())
    {
        index = freeSlots.back();
        freeSlots.pop_back();
        entitySlots[index].archetype = 0;
        entitySlots[index].row = 0;
    }
    else
    {
        index = static_cast<uint32_t>(entitySlots.size());
        entitySlots.push_back({1, 0, 0});
    }
    return {index, entitySlots[index].version};
}

void World::DestroyEntity(const Entity entity)
{
    if (!IsEntityAlive(entity))
    {
        return;
    }
    entitySlots[entity.index].version++;
    entitySlots[entity.index].archetype = kInvalidArchetype;
    freeSlots.push_back(entity.index);
}

bool World::IsEntityAlive(const Entity entity) const
{
    if (entity.index >= entitySlots.size()) return false;

    return entitySlots[entity.index].version == entity.version &&
        entitySlots[entity.index].archetype != kInvalidArchetype;
}
