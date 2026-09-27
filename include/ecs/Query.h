#pragma once
#include <cstdint>
#include <vector>

#include "ecs/Component.h"

class World;
class Archetype;

class Query
{
public:
    Query(World& world_, ComponentMask all_, ComponentMask none_ = 0);

    /** The matching archetypes, rebuilt first if the world has created any since last time. */
    const std::vector<Archetype*>& Matches();

private:
    World* world;
    ComponentMask all;
    ComponentMask none;
    std::vector<Archetype*> matches;
    uint32_t builtAtGeneration;
};
