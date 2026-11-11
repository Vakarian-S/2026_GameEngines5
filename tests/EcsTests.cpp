/*
 * Tests for the ECS, built as their own program.
 *
 * They moved out of Main.cpp once Main had to become a real program rather than a test
 * runner. Nothing here touches SDL or Vulkan, so this target builds on Win32 as well as x64.
 *
 * One program, one test: it counts its own checks and returns non zero if any of them fail.
 */

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <new>
#include <string_view>
#include <vector>

#include "core/FixedStep.h"
#include "core/FrameTimeStats.h"
#include "ecs/Archetype.h"
#include "ecs/ChunkPool.h"
#include "ecs/CommandBuffer.h"
#include "ecs/Component.h"
#include "ecs/Entity.h"
#include "ecs/Phase.h"
#include "ecs/Query.h"
#include "ecs/SystemRegistry.h"
#include "ecs/System.h"
#include "ecs/SystemContext.h"
#include "ecs/World.h"

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

/** A test system that writes its name into a log each time it runs, and keeps what its context held */
class LoggingSystem : public System
{
public:
    /**
     * @param name_ the name this system logs and prints
     * @param writes_ the components this system says it writes, for the startup note
     * @param log_ where this system writes its name on each run
     */
    LoggingSystem(const char* name_, const ComponentMask writes_, std::vector<std::string_view>* log_) :
        System(name_, 0, writes_), log(log_)
    {
    }

    void Run(const SystemContext& context) override
    {
        log->push_back(GetName());
        lastDeltaTime = context.deltaTime;
        lastHadDrawList = context.drawList != nullptr;
        currentSystemDuringRun = DEBUG::currentSystem;
    }

    /** The name DEBUG::currentSystem held while this system's last run was going on */
    const char* currentSystemDuringRun = nullptr;
    /** The deltaTime this system's last run was given */
    float lastDeltaTime = 0.0f;
    /** Whether this system's last run was given a draw list */
    bool lastHadDrawList = false;

private:
    /** The test's log, which this system writes its name into */
    std::vector<std::string_view>* log;
};

/** A test system that queues the Dead tag on one entity, so a test can see when playback applies it */
class TaggingSystem : public System
{
public:
    /** @param target_ the entity this system tags */
    explicit TaggingSystem(const Entity target_) :
        System("Tagging", 0, COMPONENTS::MaskOf<Dead>()), target(target_)
    {
    }

    void Run(const SystemContext& context) override
    {
        context.commands.AddComponent<Dead>(target);
    }

private:
    /** The entity this system tags */
    Entity target;
};

/** A test system that looks for the Dead tag on one entity, so a test can see what a later system sees */
class TagWatchingSystem : public System
{
public:
    /** @param target_ the entity this system looks at */
    explicit TagWatchingSystem(const Entity target_) : System("TagWatching", COMPONENTS::MaskOf<Dead>(), 0),
                                                       target(target_)
    {
    }

    void Run(const SystemContext& context) override
    {
        sawTag = context.world.HasComponent<Dead>(target);
    }

    /** Whether the last run found the Dead tag on the target */
    bool sawTag = true;

private:
    /** The entity this system looks at */
    Entity target;
};

/** A heap resource that counts its calls, so a test can see what a ChunkPool asks for and gives back. */
class CountingResource final : public std::pmr::memory_resource
{
public:
    /** Blocks handed out by CountingResource so far */
    int allocations = 0;
    /** Blocks CountingResource has had back so far */
    int deallocations = 0;

private:
    void* do_allocate(const std::size_t bytes, const std::size_t alignment) override
    {
        ++allocations;
        return ::operator new(bytes, std::align_val_t{alignment});
    }

    void do_deallocate(void* block, const std::size_t bytes, const std::size_t alignment) override
    {
        ++deallocations;
        ::operator delete(block, bytes, std::align_val_t{alignment});
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
    {
        return this == &other;
    }
};

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

    /** A view of an archetype's first chunk, for checks on archetypes that hold fewer rows than one chunk. */
    ChunkView FirstChunk(Archetype* archetype)
    {
        return ChunkView{archetype, &archetype->Chunks().front()};
    }
}

int main()
{
    std::cout << "EcsTests\n";

    std::cout << "\n-- entities --\n";

    World world;

    const Entity a = world.CreateEntity();
    const Entity b = world.CreateEntity();
    world.DestroyEntity(a);
    const Entity c = world.CreateEntity();


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
    Check("the free list holds after create, destroy and reuse", world.CheckFreeList());

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

    /* Declared before the archetype, so the archetype gives its chunks back before the pool dies. */
    WorldFrameStats archetypeStats;
    ChunkPool archetypePool(std::pmr::new_delete_resource(), 16 * 1024, archetypeStats);
    Archetype archetype(COMPONENTS::MaskOf<Position, Velocity, Dead>(), std::pmr::new_delete_resource(),
                        &archetypePool);

    Check("three components, two columns", archetype.ColumnCount() == 2);
    Check("a tag has no column", archetype.FindLayout(COMPONENTS::IdOf<Dead>()) == nullptr);
    Check("a component not in the mask has no column", archetype.FindLayout(COMPONENTS::IdOf<Health>()) == nullptr);
    Check("starts empty", archetype.Count() == 0 && archetype.Chunks().empty());

    const Entity e1 = world.CreateEntity();
    const Entity e2 = world.CreateEntity();
    const Entity e3 = world.CreateEntity();

    Check("first row is 0", archetype.AddRow(e1) == 0);
    Check("second row is 1", archetype.AddRow(e2) == 1);
    Check("third row is 2", archetype.AddRow(e3) == 2);
    Check("three rows", archetype.Count() == 3);

    Check("three rows fit in one chunk", archetype.Chunks().size() == 1);

    Position* positions = FirstChunk(&archetype).Column<Position>();
    positions[0] = {1.0f, 0.0f, 0.0f};
    positions[1] = {2.0f, 0.0f, 0.0f};
    positions[2] = {3.0f, 0.0f, 0.0f};

    Check("writes land in the column", FirstChunk(&archetype).Column<Position>()[2].x == 3.0f);

    const Entity moved = archetype.RemoveRow(0);

    Check("the last row moved down", moved.index == e3.index && moved.version == e3.version);
    Check("count dropped to 2", archetype.Count() == 2);
    Check("its data moved with it", positions[0].x == 3.0f);

    Check("removing the last row moves nobody", archetype.RemoveRow(1).version == 0);
    Check("one row left", archetype.Count() == 1);
    Check("the entity column tracks the rows", FirstChunk(&archetype).Entities()[0].index == e3.index);

    archetype.RemoveRow(0);
    Check("an emptied chunk goes back to the pool",
          archetype.Chunks().empty() && archetypePool.FreeCount() == 1);

    std::cout << "\n-- archetype layout --\n";

    /* The plan's example: Position and Velocity, 12 bytes each with alignment 4, in a 16 KB chunk. */
    Check("an 8 + 12 + 12 byte row fits 511 times in 16 KB", archetype.RowsPerChunk() == 511);
    Check("the entity slice comes first, so Position starts after 511 entities",
          archetype.FindLayout(COMPONENTS::IdOf<Position>())->offset == 511 * 8);
    Check("Velocity starts after the Position slice",
          archetype.FindLayout(COMPONENTS::IdOf<Velocity>())->offset == 511 * 8 + 511 * 12);

    /* A tag-only archetype has a row of just an Entity. */
    Archetype tagOnly(COMPONENTS::MaskOf<Dead>(), std::pmr::new_delete_resource(), &archetypePool);
    Check("a tag-only row is one Entity, so 2,048 fit", tagOnly.RowsPerChunk() == 16 * 1024 / 8);

    std::cout << "\n-- archetype, rows across chunks --\n";

    Archetype spanning(COMPONENTS::MaskOf<Position, Velocity>(), std::pmr::new_delete_resource(), &archetypePool);
    const uint32_t perChunk = spanning.RowsPerChunk();

    /* One full chunk and three rows into the next. Each row's x is its row number. */
    for (uint32_t i = 0; i < perChunk + 3; ++i)
    {
        const uint32_t row = spanning.AddRow(Entity{i, 1});
        std::byte* bytes = spanning.ElementAt(COMPONENTS::IdOf<Position>(), row);
        const Position value{static_cast<float>(i), 0.0f, 0.0f};
        std::memcpy(bytes, &value, sizeof(Position));
    }

    Check("a full chunk makes the next row take a second chunk", spanning.Chunks().size() == 2);
    Check("the first chunk is full", spanning.Chunks()[0].count == perChunk);
    Check("the last chunk holds the rest", spanning.Chunks()[1].count == 3);
    Check("rows keep counting across chunks", spanning.Count() == perChunk + 3);

    const auto positionAt = [&spanning](const uint32_t row)
    {
        Position value{};
        std::memcpy(&value, spanning.ElementAt(COMPONENTS::IdOf<Position>(), row), sizeof(Position));
        return value.x;
    };
    Check("a row in the second chunk reads its own value", positionAt(perChunk + 1) == static_cast<float>(perChunk + 1));
    Check("ElementAt finds the second chunk at local row 0",
          spanning.ElementAt(COMPONENTS::IdOf<Position>(), perChunk) ==
          spanning.Chunks()[1].bytes + spanning.FindLayout(COMPONENTS::IdOf<Position>())->offset);

    /* Removing row 5 pulls the very last row, which sits in the second chunk, into the first. */
    const Entity pulled = spanning.RemoveRow(5);
    Check("a remove pulls the last row across chunks", pulled.index == perChunk + 2);
    Check("its value came with it", positionAt(5) == static_cast<float>(perChunk + 2));
    Check("the entity came with it",
          ChunkView(&spanning, &spanning.Chunks()[0]).Entities()[5].index == perChunk + 2);
    Check("the last chunk shrank", spanning.Chunks()[1].count == 2);

    const uint32_t freeBefore = archetypePool.FreeCount();
    spanning.RemoveRow(spanning.Count() - 1);
    spanning.RemoveRow(spanning.Count() - 1);
    Check("emptying the last chunk gives it back", spanning.Chunks().size() == 1 &&
          archetypePool.FreeCount() == freeBefore + 1);

    std::cout << "\n-- query, every chunk --\n";

    WorldConfig chunkConfig;
    chunkConfig.maxEntities = 2048;
    World chunkWorld(chunkConfig);
    std::vector<Entity> chunkEntities;
    Query chunkQuery(chunkWorld, COMPONENTS::MaskOf<Position, Velocity>());
    for (uint32_t i = 0; i < 1200; ++i)
    {
        const Entity spawned = chunkWorld.CreateEntity();
        chunkWorld.AddComponent<Position>(spawned, {1.0f, 0.0f, 0.0f});
        chunkWorld.AddComponent<Velocity>(spawned, {0.0f, 0.0f, 0.0f});
        chunkEntities.push_back(spawned);
    }

    uint32_t chunkCalls = 0;
    float chunkTotal = 0.0f;
    chunkQuery.ForEachChunk([&](const ChunkView chunk)
    {
        ++chunkCalls;
        const Position* chunkPositions = chunk.Column<Position>();
        for (uint32_t row = 0; row < chunk.Count(); ++row) chunkTotal += chunkPositions[row].x;
    });
    Check("1,200 rows at 511 a chunk take three chunks", chunkCalls == 3);
    Check("the loop reaches every row in every chunk", chunkTotal == 1200.0f);
    Check("GetComponent finds an entity in the last chunk",
          chunkWorld.GetComponent<Position>(chunkEntities.back())->x == 1.0f);

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
    Check("the free list holds after the row shuffles", stage.CheckFreeList());

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
    Check("the free list holds after a destroy across archetypes", stage.CheckFreeList());

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

    Query moving(queryWorld, COMPONENTS::MaskOf<Position>(), COMPONENTS::MaskOf<Dead>());

    Check("the query skips the dead archetype", moving.Matches().size() == 1);

    /* A shape that did not exist when the query was written. */
    const Entity q3 = queryWorld.CreateEntity();
    queryWorld.AddComponent<Position>(q3, {3.0f, 0.0f, 0.0f});
    queryWorld.AddComponent<Velocity>(q3, {0.0f, 0.0f, 0.0f});

    Check("a new archetype joins the query on its own", moving.Matches().size() == 2);

    /* The canonical system loop: outer over archetypes, inner over rows. */
    float total = 0.0f;
    moving.ForEachChunk([&](const ChunkView chunk)
    {
        const Position* positions = chunk.Column<Position>();

        for (uint32_t i = 0; i < chunk.Count(); ++i)
        {
            total += positions[i].x;
        }
    });

    Check("the loop reaches both live entities", total == 4.0f);

    std::cout << "\n-- component logic by Id --\n";

    /* The by-id path, which is the door command buffer playback will come through. */
    World idWorld;
    const Entity byId = idWorld.CreateEntity();

    const Position placed{7.0f, 8.0f, 9.0f};
    idWorld.AddComponentById(byId, COMPONENTS::IdOf<Position>(),
                             reinterpret_cast<const std::byte*>(&placed), sizeof(Position));

    Check("AddComponentById gives the entity the component", idWorld.HasComponent<Position>(byId));
    Check("AddComponentById copies the value", idWorld.GetComponent<Position>(byId)->y == 8.0f);

    const Position overwritten{1.0f, 2.0f, 3.0f};
    idWorld.AddComponentById(byId, COMPONENTS::IdOf<Position>(),
                             reinterpret_cast<const std::byte*>(&overwritten), sizeof(Position));

    Check("adding a component twice overwrites the value", idWorld.GetComponent<Position>(byId)->x == 1.0f);

    idWorld.AddComponentById(byId, COMPONENTS::IdOf<Dead>(), nullptr, 0);
    Check("a tag goes in by id with no bytes", idWorld.HasComponent<Dead>(byId));

    idWorld.RemoveComponentById(byId, COMPONENTS::IdOf<Position>());
    Check("RemoveComponentById takes the component away", !idWorld.HasComponent<Position>(byId));
    Check("removing by id leaves the other components alone", idWorld.HasComponent<Dead>(byId));

    idWorld.RemoveComponentById(byId, COMPONENTS::IdOf<Velocity>());
    Check("removing a component the entity does not have is a no-op",
          idWorld.DoesEntityExist(byId) && idWorld.HasComponent<Dead>(byId));

    const Entity staleById{byId.index, byId.version + 1};
    idWorld.AddComponentById(staleById, COMPONENTS::IdOf<Position>(),
                             reinterpret_cast<const std::byte*>(&placed), sizeof(Position));

    Check("a stale handle is ignored by id too", !idWorld.HasComponent<Position>(byId));

    std::cout << "\n-- command buffer, destroy --\n";

    World cmdWorld;
    const Entity keep = cmdWorld.CreateEntity();
    const Entity first = cmdWorld.CreateEntity();
    const Entity second = cmdWorld.CreateEntity();
    cmdWorld.AddComponent<Position>(first, {1.0f, 0.0f, 0.0f});
    cmdWorld.AddComponent<Position>(second, {2.0f, 0.0f, 0.0f});

    CommandBuffer commands;
    commands.DestroyEntity(first);
    commands.DestroyEntity(second);

    Check("recording changes nothing",
          cmdWorld.DoesEntityExist(first) && cmdWorld.DoesEntityExist(second));
    Check("the buffer holds both commands", commands.Count() == 2);

    commands.Playback(cmdWorld);

    Check("playback destroys the queued entities",
          !cmdWorld.DoesEntityExist(first) && !cmdWorld.DoesEntityExist(second));
    Check("playback leaves other entities alone", cmdWorld.DoesEntityExist(keep));
    Check("playback empties the buffer", commands.Count() == 0);

    const Entity reusedA = cmdWorld.CreateEntity();
    const Entity reusedB = cmdWorld.CreateEntity();

    commands.DestroyEntity(first);
    commands.DestroyEntity(second);
    commands.Playback(cmdWorld);

    Check("a stale handle does not destroy whoever reused its index",
          cmdWorld.DoesEntityExist(reusedA) && cmdWorld.DoesEntityExist(reusedB));

    commands.Playback(cmdWorld);
    Check("playing back an empty buffer does nothing", cmdWorld.DoesEntityExist(keep));

    const Entity spared = cmdWorld.CreateEntity();
    commands.DestroyEntity(spared);
    commands.Clear();
    commands.Playback(cmdWorld);

    Check("Clear drops commands without applying them", cmdWorld.DoesEntityExist(spared));
    Check("Clear empties the buffer", commands.Count() == 0);
    Check("the free list holds after destroys through the buffer", cmdWorld.CheckFreeList());

    std::cout << "\n-- command buffer, remove --\n";

    World removeWorld;
    const Entity shrinking = removeWorld.CreateEntity();
    removeWorld.AddComponent<Position>(shrinking, {5.0f, 6.0f, 7.0f});
    removeWorld.AddComponent<Velocity>(shrinking, {1.0f, 1.0f, 1.0f});
    removeWorld.AddComponent<Dead>(shrinking);

    CommandBuffer removals;
    removals.RemoveComponent<Velocity>(shrinking);

    Check("a queued removal leaves the component in place",
          removeWorld.HasComponent<Velocity>(shrinking));

    removals.Playback(removeWorld);

    Check("playback removes the component", !removeWorld.HasComponent<Velocity>(shrinking));
    Check("playback leaves the other components alone",
          removeWorld.HasComponent<Position>(shrinking) && removeWorld.HasComponent<Dead>(shrinking));
    Check("the surviving component keeps its value",
          removeWorld.GetComponent<Position>(shrinking)->z == 7.0f);

    removals.RemoveComponent<Velocity>(shrinking);
    removals.Playback(removeWorld);

    Check("removing a component the entity does not have is a no-op",
          removeWorld.HasComponent<Position>(shrinking));

    removals.RemoveComponent<Dead>(shrinking);
    removals.RemoveComponent<Position>(shrinking);

    Check("two removals queue as two commands", removals.Count() == 2);

    removals.Playback(removeWorld);

    Check("both removals land in one playback",
          !removeWorld.HasComponent<Dead>(shrinking) && !removeWorld.HasComponent<Position>(shrinking));
    Check("the entity survives losing every component", removeWorld.DoesEntityExist(shrinking));

    const Entity goneBeforePlayback = removeWorld.CreateEntity();
    removeWorld.AddComponent<Position>(goneBeforePlayback, {0.0f, 0.0f, 0.0f});
    removals.RemoveComponent<Position>(goneBeforePlayback);
    removeWorld.DestroyEntity(goneBeforePlayback);
    removals.Playback(removeWorld);

    Check("a removal on an entity destroyed before playback is ignored",
          !removeWorld.DoesEntityExist(goneBeforePlayback));
    Check("the free list holds after a removal on a destroyed entity", removeWorld.CheckFreeList());

    std::cout << "\n-- command buffer, add --\n";

    World addWorld;
    const Entity growing = addWorld.CreateEntity();

    CommandBuffer additions;
    additions.AddComponent<Position>(growing, {1.0f, 2.0f, 3.0f});

    Check("a queued add leaves the entity alone", !addWorld.HasComponent<Position>(growing));

    additions.Playback(addWorld);

    Check("playback adds the component", addWorld.HasComponent<Position>(growing));
    Check("playback copies the value", addWorld.GetComponent<Position>(growing)->y == 2.0f);

    additions.AddComponent<Dead>(growing);
    additions.Playback(addWorld);

    Check("a tag goes through the buffer with no payload", addWorld.HasComponent<Dead>(growing));

    additions.AddComponent<Position>(growing, {9.0f, 9.0f, 9.0f});
    additions.Playback(addWorld);

    Check("adding a component the entity already has overwrites it",
          addWorld.GetComponent<Position>(growing)->x == 9.0f);

    /* Values of three sizes with a tag between them, so a bad offset shows up as a wrong value. */
    const Entity mixedA = addWorld.CreateEntity();
    const Entity mixedB = addWorld.CreateEntity();

    additions.AddComponent<Position>(mixedA, {1.0f, 1.0f, 1.0f});
    additions.AddComponent<Dead>(mixedB);
    additions.AddComponent<Health>(mixedB, {30.0f, 100.0f});
    additions.AddComponent<Velocity>(mixedA, {2.0f, 2.0f, 2.0f});

    Check("four adds queue as four commands", additions.Count() == 4);

    additions.Playback(addWorld);

    Check("each entity got its own components",
          addWorld.HasComponent<Position>(mixedA) && addWorld.HasComponent<Velocity>(mixedA) &&
          addWorld.HasComponent<Health>(mixedB) && addWorld.HasComponent<Dead>(mixedB));
    Check("the first value in the payload is intact",
          addWorld.GetComponent<Position>(mixedA)->x == 1.0f);
    Check("a value of a different size keeps its own bytes",
          addWorld.GetComponent<Health>(mixedB)->current == 30.0f &&
          addWorld.GetComponent<Health>(mixedB)->max == 100.0f);
    Check("a value recorded after a tag keeps its offset",
          addWorld.GetComponent<Velocity>(mixedA)->z == 2.0f);

    const Entity goneBeforeAdd = addWorld.CreateEntity();
    additions.AddComponent<Position>(goneBeforeAdd, {4.0f, 4.0f, 4.0f});
    addWorld.DestroyEntity(goneBeforeAdd);
    additions.Playback(addWorld);

    Check("an add on an entity destroyed before playback is ignored",
          !addWorld.DoesEntityExist(goneBeforeAdd));
    Check("the free list holds after an add on a destroyed entity", addWorld.CheckFreeList());

    additions.AddComponent<Velocity>(mixedB, {5.0f, 5.0f, 5.0f});
    additions.RemoveComponent<Dead>(mixedB);
    additions.Playback(addWorld);

    Check("an add and a remove in one buffer both land",
          addWorld.HasComponent<Velocity>(mixedB) && !addWorld.HasComponent<Dead>(mixedB));

    std::cout << "\n-- command buffer, the hazard --\n";

    World frameWorld;
    const Entity zeroFirst = frameWorld.CreateEntity();
    const Entity healthy = frameWorld.CreateEntity();
    const Entity wounded = frameWorld.CreateEntity();
    const Entity zeroLast = frameWorld.CreateEntity();

    frameWorld.AddComponent<Health>(zeroFirst, {0.0f, 100.0f});
    frameWorld.AddComponent<Health>(healthy, {50.0f, 100.0f});
    frameWorld.AddComponent<Health>(wounded, {30.0f, 100.0f});
    frameWorld.AddComponent<Health>(zeroLast, {0.0f, 100.0f});

    Query living(frameWorld, COMPONENTS::MaskOf<Health>(), COMPONENTS::MaskOf<Dead>());

    CommandBuffer frame;
    uint32_t rowsVisited = 0;

    living.ForEachChunk([&](const ChunkView chunk)
    {
        const Health* health = chunk.Column<Health>();
        const Entity* rows = chunk.Entities();

        for (uint32_t i = 0; i < chunk.Count(); ++i)
        {
            ++rowsVisited;
            if (health[i].current <= 0.0f) frame.AddComponent<Dead>(rows[i]);
        }
    });

    Check("the loop visits every row", rowsVisited == 4);
    Check("the loop changed nothing while it ran",
          !frameWorld.HasComponent<Dead>(zeroFirst) && !frameWorld.HasComponent<Dead>(zeroLast));

    frame.Playback(frameWorld);

    Check("every entity at zero health is tagged",
          frameWorld.HasComponent<Dead>(zeroFirst) && frameWorld.HasComponent<Dead>(zeroLast));
    Check("entities above zero are left alone",
          !frameWorld.HasComponent<Dead>(healthy) && !frameWorld.HasComponent<Dead>(wounded));

    /* A second frame on the same buffer, where the loop destroys what the first one tagged. */
    Query dying(frameWorld, COMPONENTS::MaskOf<Dead>(), 0);

    dying.ForEachChunk([&](const ChunkView chunk)
    {
        const Entity* rows = chunk.Entities();

        for (uint32_t i = 0; i < chunk.Count(); ++i)
        {
            frame.DestroyEntity(rows[i]);
        }
    });

    Check("the second frame queued only its own commands", frame.Count() == 2);

    frame.Playback(frameWorld);

    Check("the tagged entities are gone",
          !frameWorld.DoesEntityExist(zeroFirst) && !frameWorld.DoesEntityExist(zeroLast));
    Check("the untagged entities survived two frames",
          frameWorld.DoesEntityExist(healthy) && frameWorld.DoesEntityExist(wounded));
    Check("the buffer is empty after the second playback", frame.Count() == 0);
    Check("the free list holds after destroys queued inside a loop", frameWorld.CheckFreeList());

    std::cout << "\n-- command buffer, create --\n";

    World createWorld;
    CommandBuffer spawns;

    const Entity firstPlaceholder = spawns.CreateEntity();
    const Entity secondPlaceholder = spawns.CreateEntity();

    Check("a placeholder carries the placeholder bit",
          (firstPlaceholder.index & kPlaceholderBit) != 0);
    Check("placeholders are numbered from zero",
          (firstPlaceholder.index & ~kPlaceholderBit) == 0 &&
          (secondPlaceholder.index & ~kPlaceholderBit) == 1);
    Check("a placeholder does not exist in the world",
          !createWorld.DoesEntityExist(firstPlaceholder));
    Check("a placeholder has no components",
          createWorld.GetComponent<Position>(firstPlaceholder) == nullptr);
    Check("two creates queue as two commands", spawns.Count() == 2);

    spawns.Playback(createWorld);

    std::vector<Archetype*> spawnedArchetypes;
    createWorld.CollectMatching(0, 0, spawnedArchetypes);

    uint32_t spawnedRows = 0;
    for (const Archetype* a : spawnedArchetypes) spawnedRows += a->Count();

    Check("playback creates one entity per create command", spawnedRows == 2);
    Check("playback empties the buffer", spawns.Count() == 0);

    const Entity afterPlayback = spawns.CreateEntity();

    Check("the next round starts placeholders back at zero",
          (afterPlayback.index & ~kPlaceholderBit) == 0);

    spawns.Clear();

    std::cout << "\n-- command buffer, placeholders resolve --\n";

    World resolveWorld;
    CommandBuffer building;

    const Entity built = building.CreateEntity();
    building.AddComponent<Position>(built, {11.0f, 12.0f, 13.0f});
    building.AddComponent<Health>(built, {75.0f, 100.0f});
    building.AddComponent<Dead>(built);

    Check("create and three adds queue as four commands", building.Count() == 4);

    building.Playback(resolveWorld);

    std::vector<Archetype*> builtArchetypes;
    resolveWorld.CollectMatching(COMPONENTS::MaskOf<Position, Health, Dead>(), 0, builtArchetypes);

    Check("every queued component landed on one entity",
          builtArchetypes.size() == 1 && builtArchetypes[0]->Count() == 1);

    const Entity realBuilt = FirstChunk(builtArchetypes[0]).Entities()[0];

    Check("the created entity is live", resolveWorld.DoesEntityExist(realBuilt));
    Check("its first value survived", resolveWorld.GetComponent<Position>(realBuilt)->y == 12.0f);
    Check("its second value survived", resolveWorld.GetComponent<Health>(realBuilt)->current == 75.0f);

    /* Two placeholders in one buffer, to prove one ordinal cannot bleed into the other. */
    const Entity twinA = building.CreateEntity();
    const Entity twinB = building.CreateEntity();
    building.AddComponent<Position>(twinA, {1.0f, 0.0f, 0.0f});
    building.AddComponent<Position>(twinB, {2.0f, 0.0f, 0.0f});
    building.AddComponent<Velocity>(twinB, {9.0f, 0.0f, 0.0f});
    building.Playback(resolveWorld);

    std::vector<Archetype*> plainPosition;
    resolveWorld.CollectMatching(COMPONENTS::MaskOf<Position>(),
                                 COMPONENTS::MaskOf<Velocity, Health, Dead>(), plainPosition);

    Check("the first placeholder got only its own component",
          plainPosition.size() == 1 && plainPosition[0]->Count() == 1 &&
          FirstChunk(plainPosition[0]).Column<Position>()[0].x == 1.0f);

    std::vector<Archetype*> movingPosition;
    resolveWorld.CollectMatching(COMPONENTS::MaskOf<Position, Velocity>(), 0, movingPosition);

    Check("the second placeholder got its own two",
          movingPosition.size() == 1 && movingPosition[0]->Count() == 1 &&
          FirstChunk(movingPosition[0]).Column<Position>()[0].x == 2.0f &&
          FirstChunk(movingPosition[0]).Column<Velocity>()[0].x == 9.0f);

    const Entity shortLived = building.CreateEntity();
    building.AddComponent<Health>(shortLived, {5.0f, 5.0f});
    building.DestroyEntity(shortLived);
    building.Playback(resolveWorld);

    std::vector<Archetype*> healthOnly;
    resolveWorld.CollectMatching(COMPONENTS::MaskOf<Health>(), COMPONENTS::MaskOf<Position>(), healthOnly);

    uint32_t healthOnlyRows = 0;
    for (const Archetype* a : healthOnly) healthOnlyRows += a->Count();

    Check("an entity created and destroyed in one buffer does not survive", healthOnlyRows == 0);

    const Entity alreadyReal = resolveWorld.CreateEntity();
    building.AddComponent<Velocity>(alreadyReal, {3.0f, 3.0f, 3.0f});
    building.Playback(resolveWorld);

    Check("a real handle still goes through untouched",
          resolveWorld.GetComponent<Velocity>(alreadyReal)->x == 3.0f);

    std::cout << "\n-- command buffer, spawning mid-loop --\n";

    World spawnWorld;
    CommandBuffer spawnFrame;

    for (uint32_t i = 0; i < 4; ++i)
    {
        const Entity mover = spawnWorld.CreateEntity();
        spawnWorld.AddComponent<Position>(mover, {static_cast<float>(i), 0.0f, 0.0f});
        spawnWorld.AddComponent<Velocity>(mover, {1.0f, 0.0f, 0.0f});
    }

    Query movers(spawnWorld, COMPONENTS::MaskOf<Position, Velocity>(), 0);

    uint32_t moverRowsVisited = 0;

    movers.ForEachChunk([&](const ChunkView chunk)
    {
        const Position* positions = chunk.Column<Position>();

        for (uint32_t i = 0; i < chunk.Count(); ++i)
        {
            ++moverRowsVisited;

            /* Each spawn ends up in the very archetype this loop is walking. */
            const Entity trail = spawnFrame.CreateEntity();
            spawnFrame.AddComponent<Position>(trail, positions[i]);
            spawnFrame.AddComponent<Velocity>(trail, {0.0f, 0.0f, 0.0f});
        }
    });

    Check("the spawning loop visits only the rows it started with", moverRowsVisited == 4);
    Check("nothing was spawned while the loop ran", movers.Matches()[0]->Count() == 4);

    spawnFrame.Playback(spawnWorld);

    Check("every spawn landed at playback", movers.Matches()[0]->Count() == 8);

    /* A second round on the same buffer, where ordinals must start over. */
    const Entity secondRound = spawnFrame.CreateEntity();
    spawnFrame.AddComponent<Health>(secondRound, {42.0f, 42.0f});
    spawnFrame.Playback(spawnWorld);

    std::vector<Archetype*> healthHolders;
    spawnWorld.CollectMatching(COMPONENTS::MaskOf<Health>(), 0, healthHolders);

    Check("the second round resolves to its own entity",
          healthHolders.size() == 1 && healthHolders[0]->Count() == 1 &&
          FirstChunk(healthHolders[0]).Column<Health>()[0].current == 42.0f);

    /* ===== Counters ===== */

    World statsWorld;
    statsWorld.ResetFrameStats();

    const Entity counted = statsWorld.CreateEntity();
    Check("creating an entity counts one", statsWorld.Stats().entitiesCreated == 1);
    Check("the empty archetype counts as created", statsWorld.Stats().archetypesCreated == 1);

    statsWorld.AddComponent<Position>(counted, {1.0f, 2.0f, 3.0f});
    Check("adding a component counts one add", statsWorld.Stats().componentsAdded == 1);
    Check("adding a component moves the entity once", statsWorld.Stats().migrations == 1);
    Check("the archetype the entity moved into counts as created", statsWorld.Stats().archetypesCreated == 2);
    Check("moving out of the empty archetype copies nothing", statsWorld.Stats().bytesCopied == 0);

    statsWorld.AddComponent<Velocity>(counted, {0.0f, 0.0f, 0.0f});
    Check("the second add carries the first component across",
          statsWorld.Stats().bytesCopied == sizeof(Position));

    statsWorld.AddComponent<Position>(counted, {9.0f, 9.0f, 9.0f});
    Check("adding a component twice still counts the call", statsWorld.Stats().componentsAdded == 3);
    Check("adding a component twice moves nothing", statsWorld.Stats().migrations == 2);

    statsWorld.RemoveComponent<Velocity>(counted);
    Check("removing a component counts one remove", statsWorld.Stats().componentsRemoved == 1);

    const WorldStats before = statsWorld.Snapshot();
    Check("the snapshot sees the live entity", before.liveEntities == 1);
    Check("the snapshot counts every archetype", before.archetypeCount == 3);

    statsWorld.ResetFrameStats();
    Check("the reset clears the counters",
          statsWorld.Stats().entitiesCreated == 0 && statsWorld.Stats().migrations == 0);
    Check("the reset leaves the snapshot alone", statsWorld.Snapshot().archetypeCount == 3);

    statsWorld.DestroyEntity(counted);
    const WorldStats after = statsWorld.Snapshot();

    Check("destroying an entity counts one", statsWorld.Stats().entitiesDestroyed == 1);
    Check("the snapshot loses the live entity", after.liveEntities == 0);
    Check("every archetype is empty once the entity is gone", after.emptyArchetypes == 3);
    Check("the slot table keeps the dead slot", after.slotTableSize == 1);
    Check("the free list holds the one dead slot", statsWorld.CheckFreeList());

    World bufferWorld;
    CommandBuffer statsBuffer;

    const Entity placeholder = statsBuffer.CreateEntity();
    statsBuffer.AddComponent<Position>(placeholder, {1.0f, 1.0f, 1.0f});
    statsBuffer.Playback(bufferWorld);
    Check("playback counts through the world it played into",
          bufferWorld.Stats().entitiesCreated == 1 && bufferWorld.Stats().componentsAdded == 1);

    std::cout << "\n-- chunk pool --\n";

    CountingResource chunkMemory;
    {
        constexpr uint32_t kTestChunkBytes = 1024;
        WorldFrameStats poolStats;
        ChunkPool pool(&chunkMemory, kTestChunkBytes, poolStats);

        Check("the pool reports its chunk size", pool.ChunkBytes() == kTestChunkBytes);
        Check("a new pool has taken nothing", poolStats.chunksTaken == 0 && chunkMemory.allocations == 0);

        std::byte* first = pool.TakeChunk();
        std::byte* second = pool.TakeChunk();
        Check("an empty free list asks the resource for each chunk", chunkMemory.allocations == 2);
        Check("two takes give two different chunks", first != nullptr && second != nullptr && first != second);
        Check("a chunk is aligned like plain new",
              reinterpret_cast<std::uintptr_t>(first) % alignof(std::max_align_t) == 0);

        pool.ReturnChunk(first);
        pool.ReturnChunk(second);
        Check("both returned chunks are free", pool.FreeCount() == 2);
        Check("every return is counted in the frame stats", poolStats.chunksReturned == 2);

        std::byte* reusedSecond = pool.TakeChunk();
        std::byte* reusedFirst = pool.TakeChunk();
        Check("the chunk returned last comes back first", reusedSecond == second && reusedFirst == first);
        Check("a reused chunk asks the resource for nothing", chunkMemory.allocations == 2);
        Check("the free list is empty again", pool.FreeCount() == 0);
        Check("every take is counted in the frame stats, new or reused", poolStats.chunksTaken == 4);

        std::byte* third = pool.TakeChunk();
        Check("an empty free list asks the resource again", chunkMemory.allocations == 3 && third != first &&
              third != second);

        pool.ReturnChunk(reusedFirst);
        pool.ReturnChunk(reusedSecond);
        pool.ReturnChunk(third);
        Check("nothing goes back to the resource while the pool lives", chunkMemory.deallocations == 0);
    }
    Check("the pool gives every free chunk back when it dies", chunkMemory.deallocations == 3);

    std::cout << "\n-- chunk counters in a world --\n";

    World chunkStatsWorld;
    chunkStatsWorld.ResetFrameStats();

    const Entity chunkUser = chunkStatsWorld.CreateEntity();
    Check("the first row of an archetype takes a chunk", chunkStatsWorld.Stats().chunksTaken == 1);

    chunkStatsWorld.AddComponent<Position>(chunkUser, {1.0f, 0.0f, 0.0f});
    Check("a migration takes a chunk in the new archetype", chunkStatsWorld.Stats().chunksTaken == 2);
    Check("and the old archetype, now empty, gives its chunk back", chunkStatsWorld.Stats().chunksReturned == 1);
    Check("the snapshot sees the free chunk", chunkStatsWorld.Snapshot().chunksFree == 1);

    const Entity secondUser = chunkStatsWorld.CreateEntity();
    Check("the next create reuses the free chunk", chunkStatsWorld.Snapshot().chunksFree == 0 &&
          chunkStatsWorld.Stats().chunksTaken == 3);

    const WorldFrameStats chunkFrame = chunkStatsWorld.Stats();
    chunkStatsWorld.ResetFrameStats();
    Check("a reset clears the chunk counters",
          chunkStatsWorld.Stats().chunksTaken == 0 && chunkStatsWorld.Stats().chunksReturned == 0);
    Check("the run totals keep them", chunkStatsWorld.Totals().chunksTaken == chunkFrame.chunksTaken &&
          chunkStatsWorld.Totals().chunksReturned == chunkFrame.chunksReturned);

    chunkStatsWorld.DestroyEntity(secondUser);
    const WorldFrameStats destroyBefore = chunkStatsWorld.Stats();
    chunkStatsWorld.DestroyEntity(chunkUser);
    Check("StatsSince carries the chunk counters",
          StatsSince(destroyBefore, chunkStatsWorld.Stats()).chunksReturned == 1);

    std::cout << "\n-- memory, a world in an arena --\n";

    WorldConfig arenaConfig;
    arenaConfig.maxEntities = 100;
    arenaConfig.arenaBytes = 64 * 1024;
    World arenaWorld(arenaConfig);

    const Entity firstInArena = arenaWorld.CreateEntity();
    arenaWorld.AddComponent<Position>(firstInArena, {1.0f, 2.0f, 3.0f});

    std::vector<Archetype*> arenaArchetypes;
    arenaWorld.CollectMatching(COMPONENTS::MaskOf<Position>(), 0, arenaArchetypes);
    const Position* columnBefore = FirstChunk(arenaArchetypes[0]).Column<Position>();
    const std::size_t usedBefore = arenaWorld.MemoryStats().arena.used;

    for (int i = 1; i < 100; ++i)
    {
        const Entity next = arenaWorld.CreateEntity();
        arenaWorld.AddComponent<Position>(next, {static_cast<float>(i), 0.0f, 0.0f});
    }

    const WorldMemoryStats arenaMemory = arenaWorld.MemoryStats();

    Check("the world took the slab it was asked for", arenaMemory.arena.capacity == 64 * 1024);
    Check("the world lives inside the slab", arenaMemory.arena.used > 0);
    Check("nothing fell back to the heap", arenaMemory.resource.fromHeap == 0);
    Check("filling a chunk takes no more memory", arenaMemory.arena.used == usedBefore);
    Check("a chunk's column does not move as rows are added",
          FirstChunk(arenaArchetypes[0]).Column<Position>() == columnBefore);
    Check("the column holds every row", arenaArchetypes[0]->Count() == 100);
    Check("a chunk starts aligned like plain new",
          reinterpret_cast<std::uintptr_t>(arenaArchetypes[0]->Chunks()[0].bytes) %
          __STDCPP_DEFAULT_NEW_ALIGNMENT__ == 0);
    Check("a column starts aligned for its own type",
          reinterpret_cast<std::uintptr_t>(columnBefore) % alignof(Position) == 0);
    Check("the first value survived", arenaWorld.GetComponent<Position>(firstInArena)->y == 2.0f);

    /* PLAN.md step 4 measures this number before and after the free list change. */
    WorldConfig reserveConfig;
    reserveConfig.maxEntities = 4096;
    reserveConfig.arenaBytes = 1024 * 1024;
    const World reserveWorld(reserveConfig);
    std::cout << "  a new World with maxEntities 4096 uses " << reserveWorld.MemoryStats().arena.used
              << " arena bytes\n";

    std::cout << "\n-- memory, a world on the heap --\n";

    WorldConfig heapConfig;
    heapConfig.arenaBytes = 0;
    World heapWorld(heapConfig);

    const Entity firstOnHeap = heapWorld.CreateEntity();
    heapWorld.AddComponent<Position>(firstOnHeap, {4.0f, 5.0f, 6.0f});

    /* Past the 64 slots a heap world starts with, so every container has to grow. */
    for (int i = 0; i < 200; ++i)
    {
        const Entity next = heapWorld.CreateEntity();
        heapWorld.AddComponent<Position>(next, {0.0f, 0.0f, 0.0f});
        heapWorld.AddComponent<Velocity>(next, {0.0f, 0.0f, 0.0f});
    }

    const WorldMemoryStats heapMemory = heapWorld.MemoryStats();

    Check("a heap world takes no slab", heapMemory.arena.capacity == 0);
    Check("every allocation went to the heap",
          heapMemory.resource.fromArena == 0 && heapMemory.resource.fromHeap > 0);
    Check("a heap world grows past its first reserve", heapWorld.Snapshot().liveEntities == 201);
    Check("values survive the growth", heapWorld.GetComponent<Position>(firstOnHeap)->z == 6.0f);

    std::cout << "\n-- systems --\n";

    World systemWorld;
    SystemRegistry registry(systemWorld);
    std::vector<std::string_view> log;

    /* The registry owns each system. The test keeps raw pointers to read them back while the registry lives */
    auto firstLogger = std::make_unique<LoggingSystem>("First", COMPONENTS::MaskOf<Position>(), &log);
    auto secondLogger = std::make_unique<LoggingSystem>("Second", COMPONENTS::MaskOf<Position>(), &log);
    auto thirdLogger = std::make_unique<LoggingSystem>("Third", 0, &log);
    const LoggingSystem* firstSystem = firstLogger.get();
    const LoggingSystem* thirdSystem = thirdLogger.get();
    registry.Add(Phase::Fixed, std::move(firstLogger));
    registry.Add(Phase::Fixed, std::move(secondLogger));
    registry.Add(Phase::Update, std::move(thirdLogger));

    /* Not checked. First and Second both write Position, so the print ends with one note */
    registry.Print();

    registry.RunPhase(Phase::Fixed, 1.0f / 60.0f, nullptr);
    Check("a phase runs only its own systems", log.size() == 2);
    Check("a phase runs its systems in add order", log.size() == 2 && log[0] == "First" && log[1] == "Second");
    Check("a system gets the deltaTime the phase was given", firstSystem->lastDeltaTime == 1.0f / 60.0f);
    Check("a system outside Extract gets no draw list", !firstSystem->lastHadDrawList);

    registry.RunPhase(Phase::Update, 0.5f, nullptr);
    Check("the next phase runs its own system", log.size() == 3 && log[2] == "Third");
    Check("each phase passes its own deltaTime", thirdSystem->lastDeltaTime == 0.5f);

    registry.RunPhase(Phase::Extract, 0.5f, nullptr);
    Check("a phase with no systems runs nothing", log.size() == 3);

    std::cout << "\n-- systems, playback --\n";

    World playbackWorld;
    SystemRegistry playbackRegistry(playbackWorld);
    const Entity tagged = playbackWorld.CreateEntity();
    playbackWorld.AddComponent<Position>(tagged, {1.0f, 2.0f, 3.0f});

    auto watcher = std::make_unique<TagWatchingSystem>(tagged);
    const TagWatchingSystem* watcherSystem = watcher.get();
    playbackRegistry.Add(Phase::Update, std::make_unique<TaggingSystem>(tagged));
    playbackRegistry.Add(Phase::Update, std::move(watcher));

    playbackRegistry.RunPhase(Phase::Update, 0.5f, nullptr);
    Check("a queued tag is not in the world during its own phase", !watcherSystem->sawTag);
    Check("the phase plays the buffer back when it ends", playbackWorld.HasComponent<Dead>(tagged));

    std::cout << "\n-- chunk view, const columns --\n";

    std::vector<Archetype*> taggedArchetypes;
    /* Position and Dead: the Position-only archetype is empty now, and an empty archetype has no chunk. */
    playbackWorld.CollectMatching(COMPONENTS::MaskOf<Position, Dead>(), 0, taggedArchetypes);
    ChunkView writable = FirstChunk(taggedArchetypes[0]);
    const ChunkView readOnly = writable;
    Check("a const view hands out the same column",
          readOnly.Column<Position>() == writable.Column<Position>());
    Check("a const column reads the value", readOnly.Column<Position>()[0].y == 2.0f);
    Check("a const view has no column for a missing component", readOnly.Column<Velocity>() == nullptr);

    std::cout << "\n-- create with components --\n";

    /* ===== World ===== */

    World withWorld;
    withWorld.ResetFrameStats();

    const Entity wholeEntity = withWorld.CreateEntity(Position{1.0f, 2.0f, 3.0f}, Velocity{4.0f, 5.0f, 6.0f}, Dead{});

    Check("a create with components makes a live entity", withWorld.DoesEntityExist(wholeEntity));
    Check("the entity has every component it was given",
          withWorld.HasComponent<Position>(wholeEntity) && withWorld.HasComponent<Velocity>(wholeEntity) &&
          withWorld.HasComponent<Dead>(wholeEntity));
    Check("Position holds the value it was given",
          withWorld.GetComponent<Position>(wholeEntity)->x == 1.0f && withWorld.GetComponent<Position>(wholeEntity)->z == 3.0f);
    Check("Velocity holds the value it was given",
          withWorld.GetComponent<Velocity>(wholeEntity)->x == 4.0f && withWorld.GetComponent<Velocity>(wholeEntity)->z == 6.0f);
    Check("a create with components makes zero migrations", withWorld.Stats().migrations == 0);
    Check("a create with components copies no bytes between archetypes", withWorld.Stats().bytesCopied == 0);
    Check("a create with components counts one create", withWorld.Stats().entitiesCreated == 1);
    Check("a create with components counts one add per component", withWorld.Stats().componentsAdded == 3);
    Check("only the final archetype is made, not the empty one", withWorld.Stats().archetypesCreated == 1);
    Check("a create with components takes one chunk and returns none",
          withWorld.Stats().chunksTaken == 1 && withWorld.Stats().chunksReturned == 0);

    /* The same types in another order must find the same archetype and fill the right columns. */
    const Entity reordered = withWorld.CreateEntity(Dead{}, Velocity{40.0f, 50.0f, 60.0f}, Position{10.0f, 20.0f, 30.0f});

    uint16_t wholeArchetype = 0;
    uint16_t reorderedArchetype = 0;
    uint32_t wholeRow = 0;
    uint32_t reorderedRow = 0;
    withWorld.Locate(wholeEntity, wholeArchetype, wholeRow);
    withWorld.Locate(reordered, reorderedArchetype, reorderedRow);

    Check("the order of the values does not change the archetype", wholeArchetype == reorderedArchetype);
    Check("the second entity takes the next row", wholeRow == 0 && reorderedRow == 1);
    Check("each value lands in its own column, whatever the order",
          withWorld.GetComponent<Position>(reordered)->x == 10.0f &&
          withWorld.GetComponent<Velocity>(reordered)->x == 40.0f);
    Check("the first entity's values are untouched", withWorld.GetComponent<Position>(wholeEntity)->x == 1.0f);

    /* The old path, one add at a time, must end in the same archetype. */
    const Entity stepByStep = withWorld.CreateEntity();
    withWorld.AddComponent<Position>(stepByStep, {0.0f, 0.0f, 0.0f});
    withWorld.AddComponent<Velocity>(stepByStep, {0.0f, 0.0f, 0.0f});
    withWorld.AddComponent<Dead>(stepByStep);

    uint16_t stepArchetype = 0;
    uint32_t stepRow = 0;
    withWorld.Locate(stepByStep, stepArchetype, stepRow);
    Check("one add at a time reaches the same archetype", stepArchetype == wholeArchetype);

    const Entity tagOnlyEntity = withWorld.CreateEntity(Dead{});
    Check("a tag alone makes an entity with the tag", withWorld.HasComponent<Dead>(tagOnlyEntity));
    Check("and nothing else", !withWorld.HasComponent<Position>(tagOnlyEntity));

    withWorld.DestroyEntity(wholeEntity);
    const Entity reused = withWorld.CreateEntity(Health{7.0f, 10.0f});
    Check("a create with components reuses a free slot", reused.index == wholeEntity.index);
    Check("and gives it a newer version", reused.version == wholeEntity.version + 1);
    Check("the old handle reads as gone", !withWorld.DoesEntityExist(wholeEntity));
    Check("the reused slot holds the new component", withWorld.GetComponent<Health>(reused)->current == 7.0f);
    Check("and not the old ones", !withWorld.HasComponent<Position>(reused));
    Check("the free list holds after a create with components reuses a slot", withWorld.CheckFreeList());

    /* ===== By id ===== */

    const Position idPosition{7.0f, 8.0f, 9.0f};
    const ComponentValue idValues[] = {
        {COMPONENTS::IdOf<Position>(), reinterpret_cast<const std::byte*>(&idPosition), sizeof(Position)},
        {COMPONENTS::IdOf<Dead>(), nullptr, 0}
    };
    const Entity fromIds = withWorld.CreateEntityByComponentIds(idValues);

    Check("CreateEntityByComponentIds copies a value in", withWorld.GetComponent<Position>(fromIds)->y == 8.0f);
    Check("CreateEntityByComponentIds takes a tag with no bytes", withWorld.HasComponent<Dead>(fromIds));
    Check("CreateEntityByComponentIds adds only what it was given", !withWorld.HasComponent<Velocity>(fromIds));

    const Entity fromNothing = withWorld.CreateEntityByComponentIds({});
    Check("an empty list makes a live entity with no components",
          withWorld.DoesEntityExist(fromNothing) && !withWorld.HasComponent<Position>(fromNothing) &&
          !withWorld.HasComponent<Dead>(fromNothing));

    /* ===== Command buffer ===== */

    World bufferedWorld;
    CommandBuffer bufferedSpawns;
    bufferedWorld.ResetFrameStats();

    const Entity plainFirst = bufferedSpawns.CreateEntity();
    const Entity withSecond = bufferedSpawns.CreateEntity(Position{1.0f, 0.0f, 0.0f}, Velocity{2.0f, 0.0f, 0.0f});

    Check("a queued create with components returns a placeholder", (withSecond.index & kPlaceholderBit) != 0);
    Check("plain and full creates share one placeholder count",
          (plainFirst.index & ~kPlaceholderBit) == 0 && (withSecond.index & ~kPlaceholderBit) == 1);
    Check("a create with components queues one command", bufferedSpawns.Count() == 2);

    /* Enough creates that the payload vector grows and moves while recording. */
    constexpr int kQueuedCreates = 100;
    for (int i = 0; i < kQueuedCreates; ++i)
    {
        bufferedSpawns.CreateEntity(Position{static_cast<float>(i), 0.0f, 0.0f},
                                    Velocity{0.0f, static_cast<float>(2 * i), 0.0f});
    }

    bufferedSpawns.Playback(bufferedWorld);

    Check("playback of creates with components makes zero migrations", bufferedWorld.Stats().migrations == 0);
    Check("playback counts every create", bufferedWorld.Stats().entitiesCreated == 2 + kQueuedCreates);
    Check("playback counts two adds per create with components",
          bufferedWorld.Stats().componentsAdded == 2 * (1 + kQueuedCreates));

    std::vector<Archetype*> queuedMovers;
    bufferedWorld.CollectMatching(COMPONENTS::MaskOf<Position, Velocity>(), 0, queuedMovers);

    uint32_t queuedRows = 0;
    float positionSum = 0.0f;
    float velocitySum = 0.0f;
    for (Archetype* archetype : queuedMovers)
    {
        for (Chunk& chunk : archetype->Chunks())
        {
            const ChunkView view{archetype, &chunk};
            for (uint32_t row = 0; row < view.Count(); ++row)
            {
                ++queuedRows;
                positionSum += view.Column<Position>()[row].x;
                velocitySum += view.Column<Velocity>()[row].y;
            }
        }
    }

    /* 1 from withSecond, plus 0 to 99. Velocity's y is 2i, and withSecond's is 0. */
    Check("every queued create with components lands in one archetype",
          queuedMovers.size() == 1 && queuedRows == 1 + kQueuedCreates);
    Check("every queued Position survived the payload growing", positionSum == 1.0f + 4950.0f);
    Check("every queued Velocity survived the payload growing", velocitySum == 9900.0f);

    /* A later command in the same pass can still name the placeholder. */
    bufferedWorld.ResetFrameStats();
    const Entity tagLater = bufferedSpawns.CreateEntity(Health{3.0f, 3.0f});
    bufferedSpawns.AddComponent<Dead>(tagLater);
    bufferedSpawns.Playback(bufferedWorld);

    std::vector<Archetype*> deadHealth;
    bufferedWorld.CollectMatching(COMPONENTS::MaskOf<Health, Dead>(), 0, deadHealth);
    Check("an add on the placeholder resolves to the created entity",
          deadHealth.size() == 1 && deadHealth[0]->Count() == 1 &&
          FirstChunk(deadHealth[0]).Column<Health>()[0].current == 3.0f);
    Check("only the later add migrates", bufferedWorld.Stats().migrations == 1);

    std::cout << "\n-- clear --\n";

    WorldConfig clearConfig;
    clearConfig.maxEntities = 2048;
    World clearWorld(clearConfig);

    constexpr uint32_t kClearMovers = 1000;
    constexpr uint32_t kClearTagged = 10;
    constexpr uint32_t kClearTotal = kClearMovers + kClearTagged;

    /* Fills the world with movers and a few tagged ones, and returns every handle in creation order. */
    const auto spawnClearLoad = [&clearWorld]()
    {
        std::vector<Entity> handles;
        for (uint32_t i = 0; i < kClearMovers; ++i)
        {
            handles.push_back(clearWorld.CreateEntity(Position{static_cast<float>(i), 0.0f, 0.0f},
                                                      Velocity{1.0f, 0.0f, 0.0f}));
        }
        for (uint32_t i = 0; i < kClearTagged; ++i)
        {
            handles.push_back(clearWorld.CreateEntity(Position{}, Dead{}));
        }
        return handles;
    };

    /* Counts the chunks every archetype holds right now. */
    const auto chunksInUse = [&clearWorld]()
    {
        std::vector<Archetype*> all;
        clearWorld.CollectMatching(0, 0, all);
        uint32_t total = 0;
        for (Archetype* archetype : all) total += static_cast<uint32_t>(archetype->Chunks().size());
        return total;
    };

    const std::vector<Entity> beforeClear = spawnClearLoad();
    const Entity heldHandle = beforeClear.front();

    const uint32_t generationBefore = clearWorld.ArchetypeGeneration();
    const WorldStats snapshotBefore = clearWorld.Snapshot();
    const uint32_t chunksBefore = chunksInUse();
    const std::size_t clearUsedBefore = clearWorld.MemoryStats().arena.used;
    const uint64_t clearHeapBefore = clearWorld.MemoryStats().resource.fromHeap;

    clearWorld.ResetFrameStats();
    clearWorld.Clear();
    const WorldStats snapshotCleared = clearWorld.Snapshot();

    Check("a handle from before the clear reads as gone", !clearWorld.DoesEntityExist(heldHandle));
    Check("and reads no component", clearWorld.GetComponent<Position>(heldHandle) == nullptr);
    Check("a clear leaves no live entity", snapshotCleared.liveEntities == 0);
    Check("a clear keeps every archetype", snapshotCleared.archetypeCount == snapshotBefore.archetypeCount);
    Check("and empties every one", snapshotCleared.emptyArchetypes == snapshotCleared.archetypeCount);
    Check("a clear does not change the archetype generation", clearWorld.ArchetypeGeneration() == generationBefore);
    Check("a clear keeps the slot table", snapshotCleared.slotTableSize == snapshotBefore.slotTableSize);
    Check("a clear returns every chunk in use", clearWorld.Stats().chunksReturned == chunksBefore);
    Check("and every one waits on the free list", snapshotCleared.chunksFree == snapshotBefore.chunksFree + chunksBefore);
    Check("a clear takes no arena bytes", clearWorld.MemoryStats().arena.used == clearUsedBefore);
    Check("a clear counts every entity it destroys", clearWorld.Stats().entitiesDestroyed == beforeClear.size());
    Check("the free list holds every slot after a clear",
          clearWorld.CheckFreeList() && snapshotCleared.slotTableSize == beforeClear.size());

    clearWorld.ResetFrameStats();
    const std::vector<Entity> afterClear = spawnClearLoad();

    Check("the respawn takes back the chunks the clear returned", clearWorld.Stats().chunksTaken == chunksBefore);
    Check("the respawn takes no new arena bytes", clearWorld.MemoryStats().arena.used == clearUsedBefore);
    Check("the respawn sends nothing to the heap", clearWorld.MemoryStats().resource.fromHeap == clearHeapBefore);
    Check("the respawn makes no archetype", clearWorld.ArchetypeGeneration() == generationBefore);
    Check("the respawn reuses the slot table", clearWorld.Snapshot().slotTableSize == snapshotBefore.slotTableSize);
    Check("the free list holds after the respawn takes every slot back", clearWorld.CheckFreeList());
    Check("the respawn takes the highest index first, as the vector did",
          afterClear.front().index == snapshotBefore.slotTableSize - 1);

    bool versionsAdvanced = true;
    for (const Entity& fresh : afterClear)
    {
        if (fresh.version != 2) versionsAdvanced = false;
    }
    Check("every respawned entity carries the version after the clear's bump", versionsAdvanced);
    Check("the old handle still reads as gone after the respawn", !clearWorld.DoesEntityExist(heldHandle));

    float respawnedSum = 0.0f;
    std::vector<Archetype*> respawnedMovers;
    clearWorld.CollectMatching(COMPONENTS::MaskOf<Position, Velocity>(), 0, respawnedMovers);
    for (Archetype* archetype : respawnedMovers)
    {
        for (Chunk& chunk : archetype->Chunks())
        {
            const ChunkView view{archetype, &chunk};
            for (uint32_t row = 0; row < view.Count(); ++row) respawnedSum += view.Column<Position>()[row].x;
        }
    }
    /* 0 + 1 + ... + 999 */
    Check("the respawned rows hold their new values", respawnedSum == 499500.0f);

    /* Many rounds, as a player restarting again and again would make. */
    bool freeListHeldEveryRound = true;
    for (int round = 0; round < 50; ++round)
    {
        clearWorld.Clear();
        if (!clearWorld.CheckFreeList()) freeListHeldEveryRound = false;
        spawnClearLoad();
        if (!clearWorld.CheckFreeList()) freeListHeldEveryRound = false;
    }
    Check("the free list holds after every one of 50 clear and respawn rounds", freeListHeldEveryRound);
    Check("50 more rounds take no new arena bytes", clearWorld.MemoryStats().arena.used == clearUsedBefore);
    Check("50 more rounds send nothing to the heap", clearWorld.MemoryStats().resource.fromHeap == clearHeapBefore);
    Check("50 more rounds keep the free chunk count", clearWorld.Snapshot().chunksFree == snapshotBefore.chunksFree);

    /* A second clear in a row must not free any slot twice, or two entities would share an index. */
    clearWorld.ResetFrameStats();
    clearWorld.Clear();
    const uint64_t destroyedByFirstClear = clearWorld.Stats().entitiesDestroyed;
    clearWorld.Clear();
    Check("a second clear in a row destroys nothing more",
          clearWorld.Stats().entitiesDestroyed == destroyedByFirstClear);
    Check("the free list holds after two clears in a row", clearWorld.CheckFreeList());
    const std::vector<Entity> afterDoubleClear = spawnClearLoad();

    std::vector<bool> indexSeen(clearWorld.Snapshot().slotTableSize, false);
    bool indicesDistinct = true;
    for (const Entity& fresh : afterDoubleClear)
    {
        if (fresh.index >= indexSeen.size() || indexSeen[fresh.index]) indicesDistinct = false;
        else indexSeen[fresh.index] = true;
    }
    Check("two clears in a row free each slot once", indicesDistinct && afterDoubleClear.size() == kClearTotal);
    Check("and the slot table does not grow", clearWorld.Snapshot().slotTableSize == snapshotBefore.slotTableSize);

    World emptyClearWorld;
    emptyClearWorld.Clear();
    Check("clearing an empty world is harmless", emptyClearWorld.Snapshot().liveEntities == 0 &&
          emptyClearWorld.DoesEntityExist(emptyClearWorld.CreateEntity()));
    Check("the free list holds after clearing an empty world", emptyClearWorld.CheckFreeList());

    std::cout << "\n-- free list --\n";

    World listWorld;
    std::vector<Entity> listed;
    for (int i = 0; i < 4; ++i) listed.push_back(listWorld.CreateEntity());
    Check("a world that never destroyed has an empty free list", listWorld.CheckFreeList());

    listWorld.DestroyEntity(listed[1]);
    listWorld.DestroyEntity(listed[3]);
    listWorld.DestroyEntity(listed[0]);
    Check("the free list holds three dead slots", listWorld.CheckFreeList());

    const Entity reuseFirst = listWorld.CreateEntity();
    const Entity reuseSecond = listWorld.CreateEntity();
    const Entity reuseThird = listWorld.CreateEntity();
    Check("the slot freed last comes back first",
          reuseFirst.index == 0 && reuseSecond.index == 3 && reuseThird.index == 1);
    Check("an empty free list grows the table again", listWorld.CreateEntity().index == 4);
    Check("the free list holds once every dead slot is reused", listWorld.CheckFreeList());

    /* Destroys and creates mixed across two archetypes, checked after every change. */
    std::vector<Entity> mixedLive;
    bool freeListHeldEveryChange = true;
    for (int i = 0; i < 300; ++i)
    {
        if (i % 3 == 2 && !mixedLive.empty())
        {
            const size_t victim = (static_cast<size_t>(i) * 7) % mixedLive.size();
            listWorld.DestroyEntity(mixedLive[victim]);
            mixedLive.erase(mixedLive.begin() + static_cast<std::ptrdiff_t>(victim));
        }
        else if (i % 2 == 0)
        {
            mixedLive.push_back(listWorld.CreateEntity(Position{static_cast<float>(i), 0.0f, 0.0f}));
        }
        else
        {
            mixedLive.push_back(listWorld.CreateEntity(Health{1.0f, 1.0f}));
        }
        if (!listWorld.CheckFreeList()) freeListHeldEveryChange = false;
    }
    Check("the free list holds after each of 300 mixed creates and destroys", freeListHeldEveryChange);
    Check("every handle still held is live",
          std::ranges::all_of(mixedLive, [&listWorld](const Entity e) { return listWorld.DoesEntityExist(e); }));

    std::cout << "\n-- debug watch --\n";

    World watchWorld;
    const Entity plain = watchWorld.CreateEntity(Position{1.0f, 0.0f, 0.0f});
    const Entity watchedEntity = watchWorld.CreateEntity(Position{2.0f, 0.0f, 0.0f});
    watchWorld.AddComponent<DEBUG::DebugWatch>(watchedEntity);

    Check("the tag moves the entity to its own archetype",
          watchWorld.HasComponent<DEBUG::DebugWatch>(watchedEntity) && !watchWorld.HasComponent<DEBUG::DebugWatch>(plain));

    /* The same moves a grab and a release make: a component in, then out again. */
    watchWorld.AddComponent<Velocity>(watchedEntity, {0.0f, 1.0f, 0.0f});
    Check("the tag survives a component added after it", watchWorld.HasComponent<DEBUG::DebugWatch>(watchedEntity));
    watchWorld.RemoveComponent<Velocity>(watchedEntity);
    Check("the tag survives that component's removal", watchWorld.HasComponent<DEBUG::DebugWatch>(watchedEntity));
    Check("and the data survives both moves", watchWorld.GetComponent<Position>(watchedEntity)->x == 2.0f);

    /* No debugger runs under CTest, so BreakOnWatch must return and the loop must go on. */
    Query watchQuery(watchWorld, COMPONENTS::MaskOf<Position>(), 0);
    float watchSum = 0.0f;
    uint32_t chunksVisited = 0;
    watchQuery.ForEachChunk([&](const ChunkView chunk)
    {
        ++chunksVisited;
        for (uint32_t i = 0; i < chunk.Count(); ++i) watchSum += chunk.Column<Position>()[i].x;
    });
    Check("a query that never names the tag still reaches the watched entity", watchSum == 3.0f);
    Check("and visits the watched archetype's chunk as well as the plain one", chunksVisited == 2);

    /* BreakOnWatch filters on DEBUG::currentSystem, so RunPhase must name each system while it runs. */
    Check("no system is named before any phase runs", DEBUG::currentSystem == nullptr);
    SystemRegistry watchRegistry(watchWorld);
    std::vector<std::string_view> watchLog;
    auto namedLogger = std::make_unique<LoggingSystem>("Named", 0, &watchLog);
    const LoggingSystem* namedSystem = namedLogger.get();
    watchRegistry.Add(Phase::Update, std::move(namedLogger));
    watchRegistry.RunPhase(Phase::Update, 0.5f, nullptr);
    Check("RunPhase names the system it is running",
          namedSystem->currentSystemDuringRun != nullptr &&
          std::string_view(namedSystem->currentSystemDuringRun) == "Named");
    Check("and clears the name once the phase ends", DEBUG::currentSystem == nullptr);

    std::cout << "\n-- fixed step --\n";

    /* One clock fed frame after frame, so each feed starts from the time the last one left over */
    FixedStep fixedStep;

    /* Runs TakeStep until it says no, and counts how many steps it gave */
    const auto countSteps = [&fixedStep]()
    {
        int steps = 0;
        while (fixedStep.TakeStep()) ++steps;
        return steps;
    };

    fixedStep.Add(0.016f);
    Check("16 ms is less than one step", countSteps() == 0);

    fixedStep.Add(0.017f);
    Check("the 16 ms left over plus 17 ms make one step", countSteps() == 1);

    fixedStep.Add(0.050f);
    Check("a 50 ms hitch catches up with three steps", countSteps() == 3);

    fixedStep.Add(1.0f);
    const int stallSteps = countSteps();
    std::cout << "  a 1 s stall gave " << stallSteps << " steps\n";
    Check("a 1 s stall is capped at 15 steps", stallSteps == 15);

    fixedStep.Reset();
    fixedStep.Add(0.016f);
    Check("a reset drops the time left over", countSteps() == 0);

    std::cout << "\n-- frame time stats --\n";

    /* One 240 Hz frame in nanoseconds: 240 of them make 1 second and 80 ns */
    constexpr std::uint64_t frame240Hz = 4'166'667;
    const auto near = [](const double value, const double expected) { return value > expected - 0.001 && value < expected + 0.001; };

    FrameTimeStats steadyStats;
    bool reportBeforeLast = false;
    for (int i = 0; i < 239; ++i) reportBeforeLast = steadyStats.AddFrame(frame240Hz) || reportBeforeLast;
    Check("239 frames at 240 Hz are under 1 s, so no report", !reportBeforeLast);
    Check("the 240th frame reaches 1 s and finishes a report", steadyStats.AddFrame(frame240Hz));
    const FrameTimeReport& steady = steadyStats.GetReport();
    Check("240 frames at 240 Hz read 240 fps", near(steady.fps, 240.0));
    Check("the average reads 4.167 ms", near(steady.averageMs, 4.166667));
    Check("the shortest and longest both read 4.167 ms", near(steady.shortestMs, 4.166667) && near(steady.longestMs, 4.166667));

    FrameTimeStats spikeStats;
    spikeStats.AddFrame(50'000'000);
    bool spikeReport = false;
    for (int i = 0; i < 228 && !spikeReport; ++i) spikeReport = spikeStats.AddFrame(frame240Hz);
    const FrameTimeReport& spike = spikeStats.GetReport();
    Check("a 50 ms frame and 228 short ones finish a report", spikeReport);
    Check("the longest reads the 50 ms spike", near(spike.longestMs, 50.0));
    Check("the shortest reads the short frames", near(spike.shortestMs, 4.166667));
    Check("the average stays near the short frames, under 4.5 ms", spike.averageMs < 4.5);

    /* steadyStats just finished a report, so this second starts from zero */
    bool tenMsReport = false;
    int tenMsFrames = 0;
    while (!tenMsReport && tenMsFrames < 1000)
    {
        tenMsReport = steadyStats.AddFrame(10'000'000);
        ++tenMsFrames;
    }
    const FrameTimeReport& afterReset = steadyStats.GetReport();
    Check("after a report, 100 frames of 10 ms make the next one", tenMsReport && tenMsFrames == 100);
    Check("the next report reads 100 fps", near(afterReset.fps, 100.0));
    Check("the next report's shortest and longest forget the last second", near(afterReset.shortestMs, 10.0) && near(afterReset.longestMs, 10.0));

    std::cout << "\n" << (failures ? Fail : Pass)
        << checks - failures << " of " << checks << " checks passed" << Reset << "\n";

    return failures == 0 ? 0 : 1;
}
