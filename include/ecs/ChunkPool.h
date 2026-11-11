#pragma once

#include <memory_resource>
#include <cstddef>
#include <cstdint>

#include "WorldFrameStats.h"

/**
 * Hands out fixed-size chunks to archetypes. Reuses returned chunks before asking the resource for
 * new ones, and frees every chunk on its free list when it dies.
 */
class ChunkPool
{
public:
    /**
     * @param memory_ where new chunks come from
     * @param chunkBytes_ bytes in every chunk, at least one pointer
     * @param stats_ the World's frame counters, where takes and returns are counted
     */
    explicit ChunkPool(std::pmr::memory_resource* memory_, uint32_t chunkBytes_, WorldFrameStats& stats_);
    ~ChunkPool();

    /* Two pools sharing one free list would hand out the same chunk twice. */
    ChunkPool(const ChunkPool&) = delete;
    ChunkPool(ChunkPool&&) = delete;
    ChunkPool& operator=(const ChunkPool&) = delete;
    ChunkPool& operator=(ChunkPool&&) = delete;

    /**
     * Takes a chunk, reusing the newest returned one when there is one, or asking the resource for a new one.
     * @return the chunk, aligned like plain new. Its bytes hold whatever was there before.
     */
    std::byte* TakeChunk();

    /**
    * Puts a chunk on the free list, for the next TakeChunk, and counts the return in the World's frame stats.
    * @param chunk a chunk this pool handed out. Must not be null.
    */
    void ReturnChunk(std::byte* chunk);

    /** @return Number of bytes allocated for each chunk. */
    uint32_t ChunkBytes() const { return chunkBytes; }

    /** @return chunks waiting on this pool's free list */
    uint32_t FreeCount() const { return chunksFree; }

private:
    /** Memory arena from where we are getting the chunks memory */
    std::pmr::memory_resource* memory;
    /** bytes allocated for each chunk */
    uint32_t chunkBytes;
    /** The first free chunk, or nullptr. Each free chunk's first bytes hold the next one. */
    std::byte* listHead = nullptr;

    /** The World's frame counters, where takes and returns are counted */
    WorldFrameStats& stats;
    /** How many chunks are currently free */
    std::uint32_t chunksFree = 0;
};
