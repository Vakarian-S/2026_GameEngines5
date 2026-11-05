#pragma once
#include <cstddef>
#include <memory_resource>
#include <vector>

#include "Component.h"
#include "Entity.h"

class ChunkPool;

/** One fixed-size buffer that holds the     same rows of every column of one archetype. */
struct Chunk
{
    std::byte* bytes;
    /** Rows in use in this chunk */
    uint32_t count;
};

/** Where one component type sits inside every chunk of an archetype. */
struct ColumnLayout
{
    ComponentId type;
    uint32_t elementSize;
    /** Bytes from the start of the chunk to this column's first element. */
    uint32_t offset;
};

/* A collection of entities with the same components */
class Archetype
{
public:
    /**
     * @param mask one bit per component type this archetype holds
     * @param memory where the columns and the entity list allocate
     * @param pool Chunk Pool to use as a memory pool for creating archetype chunks
     */
    explicit Archetype(ComponentMask mask, std::pmr::memory_resource* memory, ChunkPool* pool);

    ~Archetype();
    /**
     * Raw bytes of one component in one row, or nullptr if this archetype has no such column. 
     * @param type Component Id of the component type column to search from
     * @param row row where the component lives
     * @return bytes of the component or nullptr if component is not found
     */
    std::byte* ElementAt(ComponentId type, uint32_t row);
    
    /**
     * Raw bytes of one component in one row, read only, or nullptr if this archetype has no such column. 
     * @param type Component Id of the component type column to search from
     * @param row row where the component lives
     * @return bytes of the component or nullptr if component is not found
     */
    const std::byte* ElementAt(ComponentId type, uint32_t row) const;

    /**
     * Appends a row (entity and components). The component bytes start zeroed
     * @param entity entity to add
     * @return index of the added row.
     */
    uint32_t AddRow(Entity entity);

    /**
     * Removes a row by moving the last one into it. 
     * @param row the row of the entity to delete  
     * @return The moved entity, or {0,0} if none was moved.
     */
    Entity RemoveRow(uint32_t row);

    /**
     * Returns the amount of component columns in the archetype, this does not count tags.
     * @return Amount of component columns not counting tags.
     */
    uint32_t ColumnCount() const;

    /**
     * Gets the Mask of the Component, bits based on the components that it owns (i.e 1001100)
     * @return uint64_t mask of the archetype
     */
    ComponentMask Mask() const;

    /**
     * Gets the amount of rows on the archetype
     * @return rows as an int.
     */
    uint32_t Count() const;

    /**
     * Get the column layout of the given component type
     * @param type Type of component to find
     * @return The column
     */
    ColumnLayout* FindLayout(ComponentId type);

    /**
     * Get the column layout of the given component type
     * @param type Type of component to find
     * @return Constant version of The column
     */
    const ColumnLayout* FindLayout(ComponentId type) const;

    /**
     * Releases all the currently held chunks, returning them to the pools
     */
    void ReleaseChunks();

    /** Every chunk of this archetype. Each is full except the last one. */
    std::pmr::vector<Chunk>& Chunks() { return chunks; }

    /** Every chunk of this archetype, read only. Each is full except the last one. */
    const std::pmr::vector<Chunk>& Chunks() const { return chunks; }

    /** Rows that fit in one chunk of this archetype */
    uint32_t RowsPerChunk() const { return rowsPerChunk; }

private:
    ComponentMask mask;
    /** Where each data component sits inside a chunk, in ascending id order. The same for every chunk. */
    std::pmr::vector<ColumnLayout> columns;
    /** Every chunk is full except the last one. */
    std::pmr::vector<Chunk> chunks;
    /** Rows that fit in one chunk, computed once from the chunk size and the row size */
    uint32_t rowsPerChunk = 0;
    /** Rows across every chunk */
    uint32_t rowsCount = 0;
    /** Where this archetype takes chunks from and gives them back to */
    ChunkPool* pool;
};
