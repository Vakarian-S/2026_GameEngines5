/*
 * Tests for the ECS, built as their own program.
 *
 * They moved out of Main.cpp once Main had to become a real program rather than a test
 * runner. Nothing here touches SDL or Vulkan, so this target builds on Win32 as well as x64.
 *
 * One program, one test: it counts its own checks and returns non zero if any of them fail.
 */

#include <bitset>
#include <cstdint>
#include <iostream>
#include <vector>

#include "ecs/Archetype.h"
#include "ecs/CommandBuffer.h"
#include "ecs/Component.h"
#include "ecs/Entity.h"
#include "ecs/Query.h"
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

    Query moving(queryWorld, COMPONENTS::MaskOf<Position>(), COMPONENTS::MaskOf<Dead>());

    Check("the query skips the dead archetype", moving.Matches().size() == 1);

    /* A shape that did not exist when the query was written. */
    const Entity q3 = queryWorld.CreateEntity();
    queryWorld.AddComponent<Position>(q3, {3.0f, 0.0f, 0.0f});
    queryWorld.AddComponent<Velocity>(q3, {0.0f, 0.0f, 0.0f});

    Check("a new archetype joins the query on its own", moving.Matches().size() == 2);

    /* The canonical system loop: outer over archetypes, inner over rows. */
    float total = 0.0f;
    for (Archetype* a : moving.Matches())
    {
        Position* positions = a->Column<Position>();

        for (uint32_t i = 0; i < a->Count(); ++i)
        {
            total += positions[i].x;
        }
    }

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

    for (Archetype* a : living.Matches())
    {
        Health* health = a->Column<Health>();
        const Entity* rows = a->EntityColumn();

        for (uint32_t i = 0; i < a->Count(); ++i)
        {
            ++rowsVisited;
            if (health[i].current <= 0.0f) frame.AddComponent<Dead>(rows[i]);
        }
    }

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

    for (Archetype* a : dying.Matches())
    {
        const Entity* rows = a->EntityColumn();

        for (uint32_t i = 0; i < a->Count(); ++i)
        {
            frame.DestroyEntity(rows[i]);
        }
    }

    Check("the second frame queued only its own commands", frame.Count() == 2);

    frame.Playback(frameWorld);

    Check("the tagged entities are gone",
          !frameWorld.DoesEntityExist(zeroFirst) && !frameWorld.DoesEntityExist(zeroLast));
    Check("the untagged entities survived two frames",
          frameWorld.DoesEntityExist(healthy) && frameWorld.DoesEntityExist(wounded));
    Check("the buffer is empty after the second playback", frame.Count() == 0);

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

    const Entity realBuilt = builtArchetypes[0]->EntityColumn()[0];

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
          plainPosition[0]->Column<Position>()[0].x == 1.0f);

    std::vector<Archetype*> movingPosition;
    resolveWorld.CollectMatching(COMPONENTS::MaskOf<Position, Velocity>(), 0, movingPosition);

    Check("the second placeholder got its own two",
          movingPosition.size() == 1 && movingPosition[0]->Count() == 1 &&
          movingPosition[0]->Column<Position>()[0].x == 2.0f &&
          movingPosition[0]->Column<Velocity>()[0].x == 9.0f);

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

    for (Archetype* a : movers.Matches())
    {
        const Position* positions = a->Column<Position>();

        for (uint32_t i = 0; i < a->Count(); ++i)
        {
            ++moverRowsVisited;

            /* Each spawn ends up in the very archetype this loop is walking. */
            const Entity trail = spawnFrame.CreateEntity();
            spawnFrame.AddComponent<Position>(trail, positions[i]);
            spawnFrame.AddComponent<Velocity>(trail, {0.0f, 0.0f, 0.0f});
        }
    }

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
          healthHolders[0]->Column<Health>()[0].current == 42.0f);

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

    World bufferWorld;
    CommandBuffer statsBuffer;

    const Entity placeholder = statsBuffer.CreateEntity();
    statsBuffer.AddComponent<Position>(placeholder, {1.0f, 1.0f, 1.0f});
    statsBuffer.Playback(bufferWorld);

    Check("the buffer reports the commands it played", statsBuffer.Stats().commandsPlayed == 2);
    Check("the buffer reports the placeholder it made", statsBuffer.Stats().placeholdersCreated == 1);
    Check("the buffer reports the payload it carried",
          statsBuffer.Stats().payloadBytes == sizeof(Position));
    Check("playback counts through the world it played into",
          bufferWorld.Stats().entitiesCreated == 1 && bufferWorld.Stats().componentsAdded == 1);

    std::cout << "\n" << (failures ? Fail : Pass)
        << checks - failures << " of " << checks << " checks passed" << Reset << "\n";

    return failures == 0 ? 0 : 1;
}
