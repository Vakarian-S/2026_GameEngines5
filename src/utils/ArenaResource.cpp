#include "utils/ArenaResource.h"

#include <cassert>
#include <new>

#include "utils/Arena.h"

namespace
{
    /**
     * A byte vector asks for alignment 1. Plain new still returns 8 or 16, and the ECS stores
     * floats in byte vectors, so no request is served with less than plain new would give.
     * @param alignment what the container asked for
     * @return the alignment to serve
     */
    std::size_t AtLeastDefault(const std::size_t alignment)
    {
        return alignment < __STDCPP_DEFAULT_NEW_ALIGNMENT__ ? __STDCPP_DEFAULT_NEW_ALIGNMENT__ : alignment;
    }
}

ArenaResource::ArenaResource(Arena& arena_) : arena(&arena_), assertOnFull(arena_.Stats().capacity > 0)
{
}

void* ArenaResource::do_allocate(size_t bytes, const std::size_t alignment)
{
    /* A zero byte slice of a full arena sits at the slab's end, and Owns rejects that address. */
    if (bytes == 0) bytes = 1;
    
    if (void* served = arena->Allocate(bytes, AtLeastDefault(alignment)))
    {
        ++stats.fromArena;
        return served;
    }

    /* Debug stops here so the slab gets resized rather than quietly overflowing into the heap. */
    assert(!assertOnFull && "the arena is full, raise its size");

    ++stats.fromHeap;
    stats.heapBytes += bytes;
    ++stats.heapLive;
    if (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) return ::operator new(bytes);
    return ::operator new(bytes, std::align_val_t{alignment});
}

void ArenaResource::do_deallocate(void* block, std::size_t bytes, const std::size_t alignment)
{
    /* An arena cannot free one block, so its memory stays put until the whole arena resets. */
    if (arena->Owns(block)) return;

    --stats.heapLive;
    if (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) ::operator delete(block);
    else ::operator delete(block, std::align_val_t{alignment});
}

bool ArenaResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept
{
    return this == &other;
}