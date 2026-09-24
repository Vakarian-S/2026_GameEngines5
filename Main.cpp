#include <bitset>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

#include "core/Window.h"
#include "ecs/Component.h"
#include "ecs/Entity.h"
#include "ecs/World.h"
#include "utils/MemoryMonitor.h"

/*
 * Where the engine starts.
 *
 * The MemoryMonitor walkthrough used to live in this file. It moved to
 * demo\MemoryMonitorDemo.cpp and builds as its own program, so this one is free for engine code.
 *
 * Programs in this repository:
 *   GameEngines5        this file, the engine
 *   MemoryMonitorDemo   demo\MemoryMonitorDemo.cpp, every path the monitor watches
 *   MemoryMonitorTests  tests\MemoryMonitorTests.cpp, the checks, run by scripts\run-tests.bat
 */

/*
 * Sample component types, here only to exercise the registry. Real ones will live in their
 * own header. Position and Velocity carry data; Dead is a tag, which stores nothing and
 * exists so a query can filter on it.
 */
struct Position
{
    float x, y, z;
};

struct Velocity
{
    float x, y, z;
};

struct Dead
{
};

/** Prints one named expectation and whether it held. */
static void Check(const char* what, const bool held)
{
    std::cout << (held ? "  pass  " : "  FAIL  ") << what << "\n";
}

int main()
{
    /*
     * The runtime allocates before main, so measure against this point.
     *
     * This call used to do a second job. While the monitor was a static library, it was this
     * line that pulled its operator new and operator delete into the program, because a linker
     * only takes the parts of a library that something asks for. src/utils/MemoryMonitor.cpp is
     * compiled straight into this program now, so the replacements are always here and the
     * line is only what it looks like: a starting point to measure from.
     */
    const MemoryStats baseline = MemoryMonitor::GetStats();

    std::cout << "GameEngines5\n";

    /*
     * Entities.
     *
     * Logging is on through this part to show what the directory costs. The two reserves in
     * the World constructor are the only allocations: creating and destroying entities after
     * that touches memory that already exists.
     */
    std::cout << "\n-- entities --\n";
    MemoryMonitor::SetLogging(true);

    World world;

    const Entity a = world.CreateEntity();
    const Entity b = world.CreateEntity();
    world.DestroyEntity(a);
    const Entity c = world.CreateEntity();

    MemoryMonitor::SetLogging(false);

    std::cout << "a = {" << a.index << ", " << a.version << "}\n";
    std::cout << "b = {" << b.index << ", " << b.version << "}\n";
    std::cout << "c = {" << c.index << ", " << c.version << "}\n";

    Check("a is dead once destroyed", !world.IsEntityAlive(a));
    Check("b is untouched by a's death", world.IsEntityAlive(b));
    Check("c is alive", world.IsEntityAlive(c));
    Check("c recycled a's index", c.index == a.index);
    Check("c carries a newer version", c.version == a.version + 1);

    /* Destroying a handle that is already dead does nothing rather than corrupting the slot. */
    world.DestroyEntity(a);
    Check("destroying a dead handle is harmless", world.IsEntityAlive(b) && world.IsEntityAlive(c));

    /*
     * Components.
     *
     * Ids are handed out on first use, so Position is asked for first and gets 0. Nothing
     * else in the engine cares which number a type got, only that it is always the same one.
     */
    std::cout << "\n-- components --\n";

    const ComponentId positionId = COMPONENTS::IdOf<Position>();
    const ComponentId velocityId = COMPONENTS::IdOf<Velocity>();
    const ComponentId deadId = COMPONENTS::IdOf<Dead>();

    for (const ComponentId id : {positionId, velocityId, deadId})
    {
        const ComponentInfo& info = COMPONENTS::InfoOf(id);
        std::cout << "id " << id
            << "  size " << info.size
            << "  align " << info.alignment
            << "  " << info.name << "\n";
    }

    Check("an id is the same every call", COMPONENTS::IdOf<Position>() == positionId);
    Check("different types get different ids", positionId != velocityId);
    Check("Position is 12 bytes", COMPONENTS::InfoOf(positionId).size == 12);
    Check("a tag is 0 bytes, not 1", COMPONENTS::InfoOf(deadId).size == 0);

    /*
     * Masks.
     *
     * One bit per component type. Matching a query against an archetype is two bitwise ANDs
     * over a number this size, which is why filtering is done per archetype and not per entity.
     */
    const ComponentMask positionOnly = COMPONENTS::MaskOf<Position>();
    const ComponentMask both = COMPONENTS::MaskOf<Position, Velocity>();

    std::cout << "MaskOf<Position>()           = " << std::bitset<kMaxComponents>(positionOnly) << "\n";
    std::cout << "MaskOf<Position, Velocity>() = " << std::bitset<kMaxComponents>(both) << "\n";
    std::cout << "MaskOf<>()                   = " << std::bitset<kMaxComponents>(COMPONENTS::MaskOf<>()) << "\n";

    /*
     * The parentheses are load bearing. == binds tighter than &, so writing
     * maskA & maskB == 0 would compare the masks first and then AND the result.
     */
    Check("both bits are set", both == (positionOnly | COMPONENTS::MaskOf<Velocity>()));
    Check("separate types share no bits", (positionOnly & COMPONENTS::MaskOf<Velocity>()) == 0);
    Check("an empty mask is zero", COMPONENTS::MaskOf<>() == 0);
    Check("a mask contains its own type", (both & positionOnly) == positionOnly);

    std::cout << "\n";

    try
    {
        Window window("GameEngines5", 1280, 720);

        while (window.PollEvents())
        {
            /*
             * Nothing is drawn yet, so give the core back rather than spinning on an empty
             * event queue. The renderer replaces this with the frame it presents.
             */
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    MemoryMonitor::Report(baseline);
    MemoryMonitor::ReportLeaks(baseline);

    return 0;
}
