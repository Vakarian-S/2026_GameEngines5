#pragma once

/* ============================== Switch ============================== */

/* On in Debug and Release. Set to 0 and the ECS allocates from the heap as it did before. */
#ifndef GE5_USE_ARENA
#define GE5_USE_ARENA 1
#endif

#include <cstddef>
#include <cstdint>

struct ArenaStats
{
    /** Bytes in the memory slab **/
    std::size_t capacity = 0;
    /** Bytes used since the last reset **/
    std::size_t used = 0;
    /** Maximum amount of bytes recorded at once, across every reset **/
    std::size_t highWater = 0;
    /** Bytes skipped to match alignment since the last reset **/
    std::size_t padding = 0;
    /** Count of allocation calls **/
    std::uint64_t allocations = 0;
    /** Calls that did not fit our designated slab **/
    std::uint64_t refusals = 0;
    /** Times the arena was reset */
    std::uint64_t resets = 0;

    /** @return bytes still free in the slab */
    std::size_t Free() const { return capacity - used; }
};

/**
 * One block of memory from the OS, handed out by moving a pointer forward.
 *
 * Allocating is one addition. Freeing one block is not possible at all: the whole arena is
 * released at once by Reset, or by the destructor. That refusal is where the speed comes from,
 * and the price is paid in memory, since bytes handed out stay reserved until a reset.
 *
 * Reset does not run destructors. Only put trivially destructible things in here.
 *
 * Not thread safe. One arena per thread, or a lock around it.
 */
class Arena
{
public:
    explicit Arena(std::size_t capacityBytes);
    ~Arena();

    /* One slab, released once. Nothing may copy or move the owner of it. */
    Arena(const Arena&) = delete;
    Arena(Arena&&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena& operator=(Arena&&) = delete;

    /**
     * Hands out a slice of the slab.
     *
     * The bytes are not cleared and hold whatever was there before.
     *
     * @param size bytes wanted, may be zero
     * @param alignment required alignment, a power of two
     * @return the slice, or nullptr when the rest of the slab is too small
     */
    void* Allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

    /**
     * Hands the whole slab back for reuse. Every pointer taken from it is dangling afterwards,
     * and nothing here can tell. Destructors are not run.
     */
    void Reset();

    /**
    * @param block any pointer
    * @return true if that pointer came from this arena's slab
    */
    bool Owns(const void* block) const;

    /** @return a copy of the counters */
    ArenaStats Stats() const;

private:
    std::byte* begin;
    std::byte* current;
    std::byte* end;

    std::size_t highWater = 0;
    std::size_t padding = 0;
    std::uint64_t allocations = 0;
    std::uint64_t refusals = 0;
    std::uint64_t resets = 0;
};
