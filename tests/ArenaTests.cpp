/*
 * Tests for the arena allocator, built as their own program.
 *
 * Nothing here touches SDL, Vulkan or the ECS, so this target builds on Win32 as well as x64.
 *
 * One program, one test: it counts its own checks and returns non zero if any of them fail.
 */

#include <cstddef>
#include <cstdint>
#include <iostream>

#include "utils/Arena.h"

namespace
{
    int checks = 0;
    int failures = 0;

    constexpr const char* Pass = "\x1b[1;32m";
    constexpr const char* Fail = "\x1b[1;91m";
    constexpr const char* Reset = "\x1b[0m";

    /** Prints one named expectation, and remembers whether it held. */
    void Check(const char* what, const bool held)
    {
        ++checks;
        if (!held) ++failures;
        std::cout << (held ? "  pass  " : "  FAIL  ") << what << "\n";
    }

    /** @return true if a pointer sits on a multiple of the alignment */
    bool IsAligned(const void* block, const std::size_t alignment)
    {
        return reinterpret_cast<std::uintptr_t>(block) % alignment == 0;
    }
}

int main()
{
    std::cout << "ArenaTests\n\n";

    std::cout << "TEST CASE: a fresh arena holds its slab and nothing else\n";
    {
        const Arena arena(1024);
        const ArenaStats stats = arena.Stats();
        Check("capacity is what was asked for", stats.capacity == 1024);
        Check("nothing is used yet", stats.used == 0);
        Check("the whole slab is free", stats.Free() == 1024);
        Check("no allocations yet", stats.allocations == 0);
        Check("no refusals yet", stats.refusals == 0);
    }

    std::cout << "\nTEST CASE: allocating moves the pointer forward\n";
    {
        Arena arena(1024);
        void* first = arena.Allocate(100, 1);
        void* second = arena.Allocate(100, 1);

        Check("the first call is served", first != nullptr);
        Check("the second call is served", second != nullptr);
        Check("the two do not overlap",
              static_cast<std::byte*>(second) - static_cast<std::byte*>(first) == 100);
        Check("used is the sum of both", arena.Stats().used == 200);
        Check("both calls were counted", arena.Stats().allocations == 2);
    }

    std::cout << "\nTEST CASE: bytes handed out are writable and stay written\n";
    {
        Arena arena(1024);
        auto* numbers = static_cast<std::uint32_t*>(arena.Allocate(sizeof(std::uint32_t) * 4,
                                                                  alignof(std::uint32_t)));
        void* between = arena.Allocate(64, 64);
        (void)between;

        for (int i = 0; i < 4; ++i) numbers[i] = static_cast<std::uint32_t>(i + 1) * 11;

        Check("what was written is still there",
              numbers[0] == 11 && numbers[1] == 22 && numbers[2] == 33 && numbers[3] == 44);
    }

    std::cout << "\nTEST CASE: alignment is honoured, and the padding is counted\n";
    {
        Arena arena(4096);

        /* One odd byte first, so every alignment below has something to skip past. */
        arena.Allocate(1, 1);

        bool allAligned = true;
        for (const std::size_t alignment : {2u, 4u, 8u, 16u, 32u, 64u, 128u})
        {
            arena.Allocate(1, 1);
            const void* block = arena.Allocate(8, alignment);
            if (block == nullptr || !IsAligned(block, alignment)) allAligned = false;
        }

        Check("every alignment from 2 to 128 lands on a multiple", allAligned);
        Check("the skipped bytes were counted as padding", arena.Stats().padding > 0);
    }

    std::cout << "\nTEST CASE: a full arena refuses instead of overrunning\n";
    {
        Arena arena(256);
        void* whole = arena.Allocate(256, 1);
        Check("a request for the whole slab fits", whole != nullptr);
        Check("the slab is now full", arena.Stats().Free() == 0);

        void* oneMore = arena.Allocate(1, 1);
        Check("one more byte is refused", oneMore == nullptr);
        Check("the refusal was counted", arena.Stats().refusals == 1);
        Check("a refusal does not move the pointer", arena.Stats().used == 256);
    }

    std::cout << "\nTEST CASE: a refusal leaves the arena usable\n";
    {
        Arena arena(256);
        arena.Allocate(200, 1);

        Check("a request that does not fit is refused", arena.Allocate(100, 1) == nullptr);
        Check("a request that does fit is still served", arena.Allocate(50, 1) != nullptr);
        Check("used counts only what was served", arena.Stats().used == 250);
        Check("one refusal is on the record", arena.Stats().refusals == 1);
    }

    std::cout << "\nTEST CASE: a request larger than the slab is refused, not clamped\n";
    {
        Arena arena(128);
        Check("asking for more than the slab is refused", arena.Allocate(129, 1) == nullptr);
        Check("asking for a huge size is refused rather than wrapping",
              arena.Allocate(static_cast<std::size_t>(-1) / 2, 1) == nullptr);
        Check("nothing was handed out", arena.Stats().used == 0);
    }

    std::cout << "\nTEST CASE: reset hands the whole slab back\n";
    {
        Arena arena(1024);
        void* before = arena.Allocate(500, 1);
        const std::size_t highWaterBefore = arena.Stats().highWater;

        arena.Reset();

        Check("used is back to zero", arena.Stats().used == 0);
        Check("the reset was counted", arena.Stats().resets == 1);
        Check("the high water mark survives the reset",
              arena.Stats().highWater == highWaterBefore && highWaterBefore == 500);

        void* after = arena.Allocate(500, 1);
        Check("the same bytes are handed out again", after == before);
    }

    std::cout << "\nTEST CASE: the high water mark remembers the worst moment\n";
    {
        Arena arena(1024);
        arena.Allocate(800, 1);
        arena.Reset();
        arena.Allocate(100, 1);

        Check("high water keeps the larger of the two", arena.Stats().highWater == 800);
        Check("used reflects only the current round", arena.Stats().used == 100);
    }

    std::cout << "\nTEST CASE: Owns tells its own memory from everything else\n";
    {
        Arena arena(256);
        const void* mine = arena.Allocate(16, 1);
        int onTheStack = 0;

        Check("a pointer from the arena is owned", arena.Owns(mine));
        Check("a stack address is not owned", !arena.Owns(&onTheStack));
        Check("nullptr is not owned", !arena.Owns(nullptr));

        Arena other(256);
        const void* theirs = other.Allocate(16, 1);
        Check("another arena's memory is not owned", !arena.Owns(theirs));
    }

    std::cout << "\nTEST CASE: Owns answers for the whole slab, not only the handed out part\n";
    {
        Arena arena(256);
        const void* mine = arena.Allocate(16, 1);
        const std::byte* tail = static_cast<const std::byte*>(mine) + 200;

        Check("a pointer into the untouched tail is still the arena's", arena.Owns(tail));

        arena.Reset();
        Check("a pointer stays owned after a reset", arena.Owns(mine));
    }

    std::cout << "\nTEST CASE: zero bytes is served without moving the pointer\n";
    {
        Arena arena(256);
        void* empty = arena.Allocate(0, 1);
        Check("a zero byte request is served", empty != nullptr);
        Check("it took no space", arena.Stats().used == 0);
    }

    std::cout << "\n" << (failures ? Fail : Pass)
        << checks - failures << " of " << checks << " checks passed" << Reset << "\n";

    return failures == 0 ? 0 : 1;
}
