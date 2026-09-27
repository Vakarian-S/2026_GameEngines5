#include "ecs/Query.h"
#include "ecs/World.h"

Query::Query(World& world_, const ComponentMask all_, const ComponentMask none_)
    : world(&world_), all(all_), none(none_), builtAtGeneration(0)
{
}

const std::vector<Archetype*>& Query::Matches()
{
    if (builtAtGeneration != world->ArchetypeGeneration())
    {
        world->CollectMatching(all, none, matches);
        builtAtGeneration = world->ArchetypeGeneration();
    }
    return matches;
}
