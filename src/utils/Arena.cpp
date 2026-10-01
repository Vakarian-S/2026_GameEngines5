#include "utils/Arena.h"

#include <cassert>
#include <new>

namespace
{
    /**
     * Rounds a pointer up to the next multiple of an alignment.
     * @param ptr where to start
     * @param alignment a power of two
     * @return the first address at or after pointer that satisfies the alignment
     */
    std::byte* AlignUp(std::byte* ptr, const std::size_t alignment)
    {
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(ptr);
        const std::uintptr_t aligned = (address + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
        return reinterpret_cast<std::byte*>(aligned);
    }
}

Arena::Arena(const std::size_t capacityBytes) : begin(static_cast<std::byte*>(::operator new(capacityBytes))),
                                          current(begin),
                                          end(begin + capacityBytes)
{
}

Arena::~Arena()
{
    ::operator delete(begin);
}

void* Arena::Allocate(const std::size_t size, const std::size_t alignment)
{
    assert(alignment > 0 && (alignment & (alignment - 1)) == 0 && "alignment must be a power of two");

    std::byte* const aligned = AlignUp(current, alignment);

    /* Compared this way round because end - aligned would be undefined past the slab. */
    if (aligned > end || static_cast<std::size_t>(end - aligned) < size)
    {
        ++refusals;
        return nullptr;
    }

    padding += static_cast<std::size_t>(aligned - current);
    current = aligned + size;
    ++allocations;

    const std::size_t used = static_cast<std::size_t>(current - begin);
    if (used > highWater) highWater = used;

    return aligned;
}

void Arena::Reset()
{
    current = begin;
    padding = 0;
    ++resets;
}

bool Arena::Owns(const void* block) const
{
    const auto address = static_cast<const std::byte*>(block);
    return address >= begin && address < end;
}

ArenaStats Arena::Stats() const
{
    ArenaStats stats;
    stats.capacity = static_cast<std::size_t>(end - begin);
    stats.used = static_cast<std::size_t>(current - begin);
    stats.refusals = refusals;
    stats.allocations = allocations;
    stats.resets = resets;
    stats.padding = padding;
    stats.highWater = highWater;
    return stats;   
}
