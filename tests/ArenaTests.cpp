/*
 * Tests for the arena allocator and the resource that plugs it into std::pmr, built as
 * their own program.
 *
 * Nothing here touches SDL, Vulkan or the ECS, so this target builds on Win32 as well as x64.
 *
 * One program, one test: it counts its own checks and returns non zero if any of them fail.
 */

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory_resource>
#include <vector>

#include "utils/Arena.h"
#include "utils/ArenaResource.h"

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

#ifdef _DEBUG
    std::cout << "\nTEST CASE: a Debug reset poisons what was handed out\n";
    {
        Arena arena(256);
        auto* bytes = static_cast<std::uint8_t*>(arena.Allocate(16, 1));
        for (int i = 0; i < 16; ++i) bytes[i] = 0x11;

        arena.Reset();

        bool poisoned = true;
        for (int i = 0; i < 16; ++i) if (bytes[i] != 0xDD) poisoned = false;

        Check("every handed out byte reads 0xDD after the reset", poisoned);
    }
#endif
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

    std::cout << "\nTEST CASE: a pmr vector allocates from the arena\n";
    {
        Arena arena(4096);
        ArenaResource resource(arena);

        std::pmr::vector<std::byte> data(&resource);
        data.resize(100);

        Check("the arena served the vector", arena.Stats().used >= 100);
        Check("the resource counted it", resource.Stats().fromArena >= 1);
        Check("nothing fell back to the heap", resource.Stats().fromHeap == 0);
        Check("the vector holds what was asked for", data.size() == 100);
    }

    std::cout << "\nTEST CASE: the bytes behave like memory\n";
    {
        Arena arena(4096);
        ArenaResource resource(arena);

        std::pmr::vector<int> numbers(&resource);
        for (int i = 0; i < 50; ++i) numbers.push_back(i * 3);

        bool correct = true;
        for (int i = 0; i < 50; ++i) if (numbers[static_cast<std::size_t>(i)] != i * 3) correct = false;

        Check("every value survived the growth", correct);
        Check("growing reallocated more than once", resource.Stats().fromArena > 1);
    }

    std::cout << "\nTEST CASE: giving arena memory back leaves the arena alone\n";
    {
        Arena arena(4096);
        ArenaResource resource(arena);
        std::size_t usedWhileAlive = 0;

        {
            std::pmr::vector<std::byte> data(&resource);
            data.resize(200);
            usedWhileAlive = arena.Stats().used;
        }

        Check("the arena did not take the memory back", arena.Stats().used == usedWhileAlive);
        Check("no heap block was freed by mistake", resource.Stats().heapLive == 0);
    }

    std::cout << "\nTEST CASE: a full arena falls back to the heap instead of failing\n";
    {
        Arena arena(64);
        ArenaResource resource(arena);

        /* The assert would stop this test before it could check the fallback it is about. */
        resource.SetAssertOnFull(false);

        auto* data = new std::pmr::vector<std::byte>(&resource);
        data->resize(4096);

        Check("the allocation was served anyway", data->size() == 4096);
        Check("the fallback was counted", resource.Stats().fromHeap >= 1);
        Check("the fallback bytes were counted", resource.Stats().heapBytes >= 4096);
        Check("a heap block is outstanding", resource.Stats().heapLive >= 1);

        delete data;
        Check("the heap block was given back", resource.Stats().heapLive == 0);
    }

    std::cout << "\nTEST CASE: a zero byte request on a full arena is given back safely\n";
    {
        Arena arena(64);
        ArenaResource resource(arena);
        resource.SetAssertOnFull(false);

        void* whole = resource.allocate(64, 1);
        void* empty = resource.allocate(0, 1);

        Check("the zero byte request is served", empty != nullptr);
        Check("it did not come from the arena", !arena.Owns(empty));
        Check("the heap served it", resource.Stats().heapLive == 1);

        resource.deallocate(empty, 0, 1);
        Check("the heap block was given back", resource.Stats().heapLive == 0);

        resource.deallocate(whole, 64, 1);
    }

    std::cout << "\nTEST CASE: an arena with no capacity refuses everything\n";
    {
        Arena arena(0);
        Check("capacity is zero", arena.Stats().capacity == 0);
        Check("one byte is refused", arena.Allocate(1, 1) == nullptr);
        Check("zero bytes is refused too", arena.Allocate(0, 1) == nullptr);
        Check("both refusals were counted", arena.Stats().refusals == 2);
        Check("it owns nothing", !arena.Owns(nullptr));
    }

    std::cout << "\nTEST CASE: a resource over an empty arena is the heap\n";
    {
        Arena arena(0);
        ArenaResource resource(arena);
        resource.SetAssertOnFull(false);

        {
            std::pmr::vector<int> numbers(&resource);
            for (int i = 0; i < 100; ++i) numbers.push_back(i);

            Check("the values are intact", numbers[0] == 0 && numbers[99] == 99);
            Check("nothing came from the arena", resource.Stats().fromArena == 0);
            Check("the heap served every call", resource.Stats().fromHeap >= 1);
            Check("a heap block is outstanding", resource.Stats().heapLive >= 1);
        }

        Check("every heap block was given back", resource.Stats().heapLive == 0);
    }

    std::cout << "\nTEST CASE: a byte vector is aligned like plain new\n";
    {
        Arena arena(4096);
        ArenaResource resource(arena);

        /* One odd byte first, so the next slice has something to skip past. */
        arena.Allocate(1, 1);

        std::pmr::vector<std::byte> bytes(&resource);
        bytes.resize(24);

        Check("the bytes sit on the default new alignment",
              IsAligned(bytes.data(), __STDCPP_DEFAULT_NEW_ALIGNMENT__));

        resource.SetAssertOnFull(false);
        std::pmr::vector<std::byte> large(&resource);
        large.resize(8192);

        Check("the large vector missed the arena", !arena.Owns(large.data()));
        Check("heap bytes are aligned the same way",
              IsAligned(large.data(), __STDCPP_DEFAULT_NEW_ALIGNMENT__));
    }

    std::cout << "\nTEST CASE: a resource is only equal to itself\n";
    {
        Arena first(256);
        Arena second(256);
        ArenaResource a(first);
        ArenaResource b(second);

        Check("a resource equals itself", a.is_equal(a));
        Check("two resources over different arenas differ", !a.is_equal(b));
        Check("it differs from the default heap resource", !a.is_equal(*std::pmr::new_delete_resource()));
    }

    std::cout << "\n" << (failures ? Fail : Pass)
        << checks - failures << " of " << checks << " checks passed" << Reset << "\n";

    return failures == 0 ? 0 : 1;
}
