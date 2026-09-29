#include "ecs/World.h"

#include <cstring>


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

    const Entity entity{index, entitySlots[index].version};

    /* Everything is born in the empty archetype and moves out when it gains a component. */
    const uint16_t archetypeIndex = ArchetypeIndexFor(0);
    const uint32_t row = archetypes[archetypeIndex]->AddRow(entity);

    entitySlots[index].archetype = archetypeIndex;
    entitySlots[index].row = row;

    return entity;
}

void World::DestroyEntity(const Entity entity)
{
    /* A stale handle is normal, not an error. Something died while someone still held it. */
    if (!DoesEntityExist(entity))
    {
        return;
    }

    /* Read the slot before anything changes it. */
    const uint16_t archetypeIndex = entitySlots[entity.index].archetype;
    const uint32_t row = entitySlots[entity.index].row;

    /* Whoever was in the last row slid into this one and needs their slot corrected. */
    if (const auto [index, version] = archetypes[archetypeIndex]->RemoveRow(row); version != 0)
    {
        entitySlots[index].row = row;
    }

    /* The version bump is the invalidation. Every copy of this handle is now stale. */
    entitySlots[entity.index].version++;
    entitySlots[entity.index].archetype = kInvalidArchetype;
    freeSlots.push_back(entity.index);
}

bool World::DoesEntityExist(const Entity entity) const
{
    /* A default-constructed or garbage handle must answer false, not read past the end. */
    if (entity.index >= entitySlots.size())
    {
        return false;
    }

    return entitySlots[entity.index].version == entity.version &&
        entitySlots[entity.index].archetype != kInvalidArchetype;
}

bool World::Locate(const Entity entity, uint16_t& archetype, uint32_t& row) const
{
    if (!DoesEntityExist(entity))
    {
        return false;
    }

    archetype = entitySlots[entity.index].archetype;
    row = entitySlots[entity.index].row;
    return true;
}

void World::CollectMatching(const ComponentMask all, const ComponentMask none, std::vector<Archetype*>& out)
{
    out.clear();
    for (const std::unique_ptr<Archetype>& archetype : archetypes)
    {
        const ComponentMask mask = archetype->Mask();
        /*
        * (mask & all) != all    missing a required bit. AND only ever clears bits, so the
        *                        result can equal `all` only if every one of its bits was set.
        * (mask & none) != 0     carries at least one forbidden bit.
        */
        if ((mask & all) != all || (mask & none) != 0)
        {
            continue;
        }
        out.push_back(archetype.get());
    }
}

void World::AddComponentById(Entity entity, ComponentId id, const std::byte* value, uint32_t size)
{
    assert(id < kMaxComponents);
    if (!DoesEntityExist(entity)) return;

    const ComponentMask currentArchetypeMask = archetypes[entitySlots[entity.index].archetype]->Mask();
    MoveEntity(entity, currentArchetypeMask | (ComponentMask{1} << id));

    if (value == nullptr || size == 0) return;

    assert(size == COMPONENTS::InfoOf(id).size);

    /* MoveEntity changed the slot, so read it again rather than reusing what was read above. */
    const EntitySlot& slot = entitySlots[entity.index];
    if (std::byte* stored = archetypes[slot.archetype]->ElementAt(id, slot.row))
    {
        std::memcpy(stored, value, size);
    }
}

void World::RemoveComponentById(Entity entity, ComponentId id)
{
    assert(id < kMaxComponents);
    if (!DoesEntityExist(entity)) return;

    const ComponentMask currentArchetypeMask = archetypes[entitySlots[entity.index].archetype]->Mask();
    MoveEntity(entity, currentArchetypeMask & ~(ComponentMask{1} << id));
}

uint16_t World::ArchetypeIndexFor(ComponentMask mask)
{
    for (size_t i = 0; i < archetypes.size(); i++)
    {
        if (archetypes[i]->Mask() == mask)
        {
            return static_cast<uint16_t>(i);
        }
    }
    assert(archetypes.size() < kInvalidArchetype);
    archetypes.push_back(std::make_unique<Archetype>(mask));
    archetypeGeneration++;
    return static_cast<uint16_t>(archetypes.size() - 1);
}

void World::MoveEntity(const Entity entity, const ComponentMask newMask)
{
    const EntitySlot slot = entitySlots[entity.index];
    const uint16_t sourceIndex = slot.archetype;
    const uint32_t sourceRow = slot.row;
    ComponentMask sourceMask = archetypes[sourceIndex]->Mask();

    if (sourceMask == newMask) return;

    const uint16_t destinationIndex = ArchetypeIndexFor(newMask);
    const uint32_t destinationRow = archetypes[destinationIndex]->AddRow(entity);

    /* for each id where both srcMask and newMask have the bit: */
    for (ComponentId id = 0; id < kMaxComponents; id++)
    {
        const std::byte* source = archetypes[sourceIndex]->ElementAt(id, sourceRow);
        std::byte* destination = archetypes[destinationIndex]->ElementAt(id, destinationRow);
        if (source && destination)
        {
            std::memcpy(destination, source, COMPONENTS::InfoOf(id).size);
        }
    }

    auto [movedIndex, movedVersion] = archetypes[sourceIndex]->RemoveRow(sourceRow);
    if (movedVersion != 0) entitySlots[movedIndex].row = sourceRow;
    entitySlots[entity.index].archetype = destinationIndex;
    entitySlots[entity.index].row = destinationRow;
}
