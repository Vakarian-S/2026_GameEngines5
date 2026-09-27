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
     * Logging is on to show what this costs: two reserves in the constructor, then the
     * empty archetype and its entities vector on the first CreateEntity.
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

    Check("a is gone once destroyed", !world.DoesEntityExist(a));
    Check("b is untouched", world.DoesEntityExist(b));
    Check("c exists", world.DoesEntityExist(c));
    Check("c recycled a's index", c.index == a.index);
    Check("c carries a newer version", c.version == a.version + 1);

    /* Destroying a handle that is already dead does nothing rather than corrupting the slot. */
    world.DestroyEntity(a);
    Check("destroying a stale handle is harmless", world.DoesEntityExist(b) && world.DoesEntityExist(c));

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
     * Archetypes on their own. This one is not the world's, so the handles below are only
     * labels for its rows.
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
    Check("the entity column tracks the rows", archetype.EntityColumn()[0].index == e3.index);

    /*
     * World and archetypes together. A fresh world, so the row numbers are predictable.
     */
    std::cout << "\n-- world and archetypes --\n";

    World stage;

    /** Row an entity sits in, or -1 if it is gone. */
    const auto row_of = [&stage](const Entity e)
    {
        uint16_t archetypeIndex;
        uint32_t row;
        return stage.Locate(e, archetypeIndex, row) ? static_cast<int>(row) : -1;
    };

    /** Archetype an entity sits in, or -1 if it is gone. */
    const auto archetypeOf = [&stage](const Entity e)
    {
        uint16_t archetypeIndex;
        uint32_t row;
        return stage.Locate(e, archetypeIndex, row) ? static_cast<int>(archetypeIndex) : -1;
    };

    const Entity w1 = stage.CreateEntity();
    const Entity w2 = stage.CreateEntity();
    const Entity w3 = stage.CreateEntity();

    Check("new entities land in rows 0, 1, 2", row_of(w1) == 0 && row_of(w2) == 1 && row_of(w3) == 2);
    Check("they share one archetype", archetypeOf(w1) == archetypeOf(w2) && archetypeOf(w2) == archetypeOf(w3));

    /* The empty archetype has no columns, so nothing can be read out of it. */
    Check("a new entity has no components", !stage.HasComponent<Position>(w1));
    Check("and nothing to read", stage.GetComponent<Position>(w1) == nullptr);

    stage.DestroyEntity(w2);

    Check("w2 is gone", !stage.DoesEntityExist(w2));
    Check("a gone entity locates nowhere", row_of(w2) == -1);
    Check("w3 slid down into w2's row", row_of(w3) == 1);
    Check("w1 was not touched", row_of(w1) == 0);

    /* w3 is the last row now, so removing it moves nobody. */
    stage.DestroyEntity(w3);
    Check("w1 still sits in row 0", row_of(w1) == 0);

    const Entity w4 = stage.CreateEntity();

    Check("a recycled slot rejoins the empty archetype", archetypeOf(w4) == archetypeOf(w1));
    Check("and takes the free row", row_of(w4) == 1);
    
    /*
     * Adding components to an entity, with their archetypes shifting.
     */
    std::cout << "\n-- Adding Components --\n";
    
    const Entity m1 = stage.CreateEntity();
    const int emptyArchetype = archetypeOf(m1);

    stage.AddComponent<Position>(m1, {5.0f, 6.0f, 7.0f});

    Check("gaining a component moves the entity", archetypeOf(m1) != emptyArchetype);
    Check("it lands in row 0 of the new archetype", row_of(m1) == 0);
    Check("the component reads back", stage.GetComponent<Position>(m1)->x == 5.0f);
    Check("HasComponent agrees", stage.HasComponent<Position>(m1));

    /* A second move: Position has to survive being carried across. */
    stage.AddComponent<Velocity>(m1, {1.0f, 0.0f, 0.0f});
    Check("the older component came along", stage.GetComponent<Position>(m1)->x == 5.0f);

    /* Tags move an entity too, and store nothing. */
    stage.AddComponent<Dead>(m1);
    Check("a tag still moves the entity", stage.HasComponent<Dead>(m1));
    Check("and the data still survived", stage.GetComponent<Position>(m1)->x == 5.0f);
    Check("We can still read the Dead Tag", stage.HasComponent<Dead>(m1));

    stage.DestroyEntity(m1);
    const Entity m2 = stage.CreateEntity();

    Check("a recycled slot starts back in the empty archetype", archetypeOf(m2) == emptyArchetype);
    Check("and has no components", !stage.HasComponent<Position>(m2));
    
    /*
 * Queries. A fresh world, so the archetype count and generation are predictable.
 */
    std::cout << "\n-- queries --\n";

    World queryWorld;
    std::vector<Archetype*> matches;

    const Entity q1 = queryWorld.CreateEntity();
    const uint32_t afterFirstEntity = queryWorld.ArchetypeGeneration();

    const Entity q2 = queryWorld.CreateEntity();
    Check("reusing the empty archetype does not bump", queryWorld.ArchetypeGeneration() == afterFirstEntity);

    queryWorld.AddComponent<Position>(q1, {1.0f, 0.0f, 0.0f});
    Check("a new archetype bumps the generation", queryWorld.ArchetypeGeneration() == afterFirstEntity + 1);

    queryWorld.AddComponent<Position>(q2, {2.0f, 0.0f, 0.0f});
    Check("the second entity reuses it", queryWorld.ArchetypeGeneration() == afterFirstEntity + 1);

    queryWorld.CollectMatching(COMPONENTS::MaskOf<Position>(), 0, matches);
    Check("one archetype holds both", matches.size() == 1 && matches[0]->Count() == 2);

    queryWorld.AddComponent<Dead>(q2);
    Check("a tag makes its own archetype", queryWorld.ArchetypeGeneration() == afterFirstEntity + 2);

    queryWorld.CollectMatching(COMPONENTS::MaskOf<Position>(), COMPONENTS::MaskOf<Dead>(), matches);
    Check("none<Dead> excludes the dead archetype", matches.size() == 1 && matches[0]->Count() == 1);

    queryWorld.CollectMatching(COMPONENTS::MaskOf<Position>(), 0, matches);
    Check("without the filter both match", matches.size() == 2);

    queryWorld.CollectMatching(COMPONENTS::MaskOf<Position, Velocity>(), 0, matches);
    Check("all needs every bit, not any of them", matches.empty());

    queryWorld.CollectMatching(0, 0, matches);
    Check("an empty query matches every archetype", matches.size() == 3);


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
