#include "utils/ArenaResource.h"

#include <cassert>
#include <new>

#include "utils/Arena.h"

ArenaResource::ArenaResource(Arena& arena_) : arena(&arena_)
{
}

void* ArenaResource::do_allocate(const std::size_t bytes, const std::size_t alignment)
{
    if (void* served = arena->Allocate(bytes, alignment))
    {
        ++stats.fromArena;
        return served;
    }

    /* Debug stops here so the slab gets resized rather than quietly overflowing into the heap. */
    assert(!assertOnFull && "the arena is full, raise its size");

    ++stats.fromHeap;
    stats.heapBytes += bytes;
    ++stats.heapLive;
    return ::operator new(bytes, std::align_val_t{alignment});
}

void ArenaResource::do_deallocate(void* block, std::size_t bytes, const std::size_t alignment)
{
    /* An arena cannot free one block, so its memory stays put until the whole arena resets. */
    if (arena->Owns(block)) return;

    --stats.heapLive;
    ::operator delete(block, std::align_val_t{alignment});
}

bool ArenaResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept
{
    return this == &other;
}