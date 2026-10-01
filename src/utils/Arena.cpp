#include "utils/Arena.h"


#include <cassert>
#include <new>
#include <cstring>

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

Arena::Arena(const std::size_t capacityBytes)
    : begin(capacityBytes > 0 ? static_cast<std::byte*>(::operator new(capacityBytes)) : nullptr),
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

    /* An arena built with zero capacity has no slab to hand out. */
    if (begin == nullptr)
    {
        ++refusals;
        return nullptr;
    }

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
#ifdef _DEBUG
    /* A pointer kept past a reset now reads 0xDD bytes instead of another owner's data. */
    if (current > begin) std::memset(begin, 0xDD, static_cast<std::size_t>(current - begin));
#endif
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
