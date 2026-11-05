#pragma once
#include <cstdint>

#include "ecs/Archetype.h"
#include "ecs/Component.h"
#include "ecs/Entity.h"

/**
 * The rows of one chunk of one archetype, handed to a system by Query::ForEachChunk.
 * Owns nothing, and is valid only inside the call that receives it: after a structural change
 * the chunk may hold another archetype's rows.
 */
class ChunkView
{
public:
    /**
     * @param archetype_ the archetype that owns the chunk, which holds the column layout
     * @param chunk_ the chunk whose rows this view shows
     */
    ChunkView(Archetype* archetype_, Chunk* chunk_) : archetype(archetype_), chunk(chunk_) {}
    
    /** First T in this chunk, or nullptr if the archetype has no T column. */
    template <typename T>
    T* Column()
    {
        const ColumnLayout* layout = archetype->FindLayout(COMPONENTS::IdOf<T>());
        return layout != nullptr ? reinterpret_cast<T*>(chunk->bytes + layout->offset) : nullptr;
    }
    
    /** First T in this chunk, read only, or nullptr if the archetype has no T column. */
    template <typename T>
    const T* Column() const
    {
        const ColumnLayout* layout = archetype->FindLayout(COMPONENTS::IdOf<T>());
        return layout != nullptr ? reinterpret_cast<const T*>(chunk->bytes + layout->offset) : nullptr;
    }
    
    /** The entity in each row of this chunk. The entity slice always sits at offset 0. */
    const Entity* Entities() const { return reinterpret_cast<const Entity*>(chunk->bytes); }

    /** Rows in use in this chunk */
    uint32_t Count() const { return chunk->count; }

    /** The archetype this chunk belongs to */
    Archetype* Owner() const { return archetype; }
    
private:
    Archetype* archetype;
    Chunk* chunk;
};
