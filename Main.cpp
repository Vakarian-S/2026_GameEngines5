#include <bitset>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

#include "core/Window.h"
#include "ecs/Archetype.h"
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

struct Health
{
    float current, max;
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

    Check("a is dead once destroyed", !world.DoesEntityExist(a));
    Check("b is untouched by a's death", world.DoesEntityExist(b));
    Check("c is alive", world.DoesEntityExist(c));
    Check("c recycled a's index", c.index == a.index);
    Check("c carries a newer version", c.version == a.version + 1);

    /* Destroying a handle that is already dead does nothing rather than corrupting the slot. */
    world.DestroyEntity(a);
    Check("destroying a dead handle is harmless", world.DoesEntityExist(b) && world.DoesEntityExist(c));

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

    /*
     * Archetypes. Tested on its own: World does not place entities in archetypes yet, so
     * these handles are only labels for rows.
     */
    std::cout << "\n-- archetypes --\n";

    Archetype archetype(COMPONENTS::MaskOf<Position, Velocity, Dead>());

    Check("three components, two columns", archetype.ColumnCount() == 2);
    Check("a tag has no column", archetype.Column<Dead>() == nullptr);
    Check("a component not in the mask has no column", archetype.Column<Health>() == nullptr);
    Check("starts empty", archetype.Count() == 0);

    const Entity e1 = world.CreateEntity();
    const Entity e2 = world.CreateEntity();
    const Entity e3 = world.CreateEntity();

    Check("first row is 0", archetype.AddRow(e1) == 0);
    Check("second row is 1", archetype.AddRow(e2) == 1);
    Check("third row is 2", archetype.AddRow(e3) == 2);
    Check("three rows", archetype.Count() == 3);

    /* Fetched after the last AddRow: every resize can move the column. */
    Position* positions = archetype.Column<Position>();
    positions[0] = {1.0f, 0.0f, 0.0f};
    positions[1] = {2.0f, 0.0f, 0.0f};
    positions[2] = {3.0f, 0.0f, 0.0f};

    Check("writes land in the column", archetype.Column<Position>()[2].x == 3.0f);
    
    const Entity moved = archetype.RemoveRow(0);
    positions = archetype.Column<Position>();

    Check("the last row moved down", moved.index == e3.index && moved.version == e3.version);
    Check("count dropped to 2", archetype.Count() == 2);
    Check("its data moved with it", positions[0].x == 3.0f);

    Check("removing the last row moves nobody", archetype.RemoveRow(1).version == 0);
    Check("one row left", archetype.Count() == 1);
    

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
