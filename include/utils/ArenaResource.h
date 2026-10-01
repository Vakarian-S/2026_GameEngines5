#pragma once
#include <cstddef>
#include <cstdint>
#include <memory_resource>

class Arena;

/** What one ArenaResource has handed out, and how much of it missed the arena. */
struct ArenaResourceStats
{
    /** Calls the arena served */
    std::uint64_t fromArena = 0;
    /** Calls the heap served because the arena was full. Above zero means the slab is too small. */
    std::uint64_t fromHeap = 0;
    /** Bytes the heap served */
    std::size_t heapBytes = 0;
    /** Blocks from the heap not yet given back */
    std::uint64_t heapLive = 0;
};

/**
 * Lets a standard container allocate from an Arena.
 *
 * The three do_ functions are hooks. Nothing here calls them: the container calls
 * memory_resource::allocate, and that dispatches to this class. Construct one, hand its
 * address to a std::pmr container, and every allocation that container makes lands in the arena.
 *
 * It must outlive every container using it, and so must the arena behind it. A pmr container
 * keeps a bare pointer to its resource and never checks it.
 *
 * When the arena is full this falls back to the heap rather than failing, so a slab sized too
 * small costs memory and a count rather than a crash. Debug asserts first, so the size gets
 * fixed instead of ignored.
 *
 * Over an arena with no capacity every call goes to the heap, through this program's own
 * operator new, and nothing asserts. That is how a World runs with no arena. It is not
 * std::pmr::new_delete_resource, because that one allocates inside the runtime's DLL, where
 * the Memory Monitor cannot count it.
 */
class ArenaResource final : public std::pmr::memory_resource
{
public:
    /**
     * @param arena_ the arena to hand out, which must outlive this resource
     */
    explicit ArenaResource(Arena& arena_);

    /** @return a copy of the counters */
    ArenaResourceStats Stats() const { return stats; }

    /**
     * Turns the Debug assert on a full arena off. Only the fallback's own test needs this,
     * because the assert would stop that test before it could check anything.
     * @param enabled
     */
    void SetAssertOnFull(const bool enabled) { assertOnFull = enabled; }

private:
    /**
     * Hook the container calls when it needs memory. Serves from the arena, or from the heap
     * when the arena is full.
     * @param bytes bytes wanted
     * @param alignment required alignment
     * @return the memory, never nullptr
     */
    void* do_allocate(size_t bytes, std::size_t alignment) override;

    /**
     * Hook the container calls when it gives memory back. Arena memory is left where it is,
     * because an arena cannot free one block. Heap memory is freed.
     * @param block what the container is returning
     * @param bytes its size
     * @param alignment its alignment
     */
    void do_deallocate(void* block, std::size_t bytes, std::size_t alignment) override;

    /**
     * Hook the library calls to ask whether two resources are interchangeable.
     * @param other the resource to compare against
     * @return true only for this same object, since each arena holds different memory
     */
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

    Arena* arena;
    ArenaResourceStats stats;
    bool assertOnFull;
};