#pragma once
#include <cstdint>
#include <vector>

#include "ecs/debug/DebugWatch.h"
#include "ecs/Component.h"
#include "ecs/ChunkView.h"

class World;

class Query
{
public:
    Query(World& world_, ComponentMask all_, ComponentMask none_ = 0);

    /** The matching archetypes, rebuilt first if the world has created any since last time. */
    const std::vector<Archetype*>& Matches();

    /**
    * Calls function once per chunk of every matching archetype, with a ChunkView of that chunk.
    * A return inside function moves on to the next chunk.
    */
    template <typename Function>
    void ForEachChunk(Function&& function)
    {
        const ComponentMask debugWatchMask = COMPONENTS::MaskOf<DEBUG::DebugWatch>();
        for (Archetype* archetype : Matches())
        {
            const bool watched = (archetype->Mask() & debugWatchMask) != 0;
            for (Chunk& chunk : archetype->Chunks())
            {
                if (watched) [[unlikely]] DEBUG::BreakOnWatch();
                function(ChunkView{archetype, &chunk});
            }
        }
    }

private:
    World* world;
    ComponentMask all;
    ComponentMask none;
    std::vector<Archetype*> matches;
    uint32_t builtAtGeneration;
};
