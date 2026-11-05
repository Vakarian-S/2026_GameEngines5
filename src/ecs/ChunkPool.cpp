#include "ecs/ChunkPool.h"

#include <cassert>
#include <cstring>

ChunkPool::ChunkPool(std::pmr::memory_resource* memory_, const uint32_t chunkBytes_, FrameStats& stats_)
    : memory(memory_), chunkBytes(chunkBytes_), listHead(nullptr), stats(stats_)
{
    assert(chunkBytes >= sizeof(std::byte*) && "ChunkPool: Chunk size must be at least sizeof(std::byte*)");
}

ChunkPool::~ChunkPool()
{
    while (listHead != nullptr)
    {
        std::byte* next = nullptr;
        std::memcpy(&next, listHead, sizeof(std::byte*));
        memory->deallocate(listHead, chunkBytes, alignof(std::max_align_t));
        listHead = next;
    }
}

std::byte* ChunkPool::TakeChunk()
{
    GE5_ECS_COUNT(stats, chunksTaken, 1);

    if (listHead != nullptr)
    {
        /* Reuse the newest returned chunk; its first bytes hold the next free chunk. */
        std::byte* chunk = listHead;
        std::memcpy(&listHead, chunk, sizeof(std::byte*));
        chunksFree--;
        return chunk;
    }

    return static_cast<std::byte*>(memory->allocate(chunkBytes, alignof(std::max_align_t)));
}

void ChunkPool::ReturnChunk(std::byte* chunk)
{
    assert(chunk != nullptr && "ChunkPool::ReturnChunk: Cannot return a null chunk");
    std::memcpy(chunk, &listHead, sizeof(std::byte*));
    listHead = chunk;
    chunksFree++;
    GE5_ECS_COUNT(stats, chunksReturned, 1);
}
