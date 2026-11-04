# Plan

**Last updated:** 2026-11-03.

This plan hardens the ECS storage. Every archetype reserves `maxEntities` rows up front, so most
memory goes to empty archetypes, and Scene3 ran out of arena. Each spawn hops through about 8 to 9
archetypes, one component at a time. A restart destroys entities one by one. And a data breakpoint
cannot follow an entity, because its bytes move on every migration. It comes from ROADMAP.md, ECS,
Hardening, rows 1, 2, 4, 7 and 12. Rows 3, 5, 6 and 8 to 11 stay in the roadmap.

## Steps

| # | step | done when | stage |
|---|---|---|---|
| 1 | Chunks: an archetype stores its rows in fixed-size chunks, taken from a pool as rows arrive | Scene3 runs past wave 15 without filling its arena, and `EcsBench` holds under a nanosecond per entity at 100,000 rows | designed |
| 2 | Create with components: one call puts an entity in its final archetype | A spawn makes zero migrations, in the World and through the command buffer | designed |
| 3 | Clear the World in one call | R in `PhysicsScene` clears and respawns the shapes, a handle from before reads as dead, and the respawn takes no new chunks from the arena | designed |
| 4 | An intrusive free list: a free slot's `row` holds the next free index | `freeSlots` is gone, the slot tests pass unchanged, `CheckFreeList` passes after every test sequence, and the arena's used bytes drop by `maxEntities × 4` | designed |
| 5 | Entity breakpoints: a `DebugWatch` tag, checked inside `ForEachChunk` | Under Rider, every system that reaches the tagged shape stops, the plain exe runs on with the tag in place, and `EcsBench` shows no cost with no tag | designed |

**Stages**, in order. The cell says what the step is ready for, and it changes only when this file
is edited. Prototype, build and recap happen in the code and the chat; WORKFLOW.md describes them.

| stage | written when | meaning |
|---|---|---|
| outline | the plan is made | the step's idea and its reason, as it came from ROADMAP.md, not designed yet |
| designed | the design talk ends | the decisions are in the step's section, ready to build |
| done | the commit lands | built, recapped and committed. Write the commit hash in the stage cell |

A prototype's results go to `findings/`, and the step's section links to them.

When every step is done, each step's section becomes a dev-note, STATE.md is updated, and this file
starts again from the template.

**Order.** Steps 2 and 3 build on the storage from step 1. Steps 3 and 4 touch the same slot code,
so they go one after the other. Step 5 does not need chunks (see its section), so it can move
earlier if it helps.

**Possible extras,** not decided: Code cleanup row 2 (an assert when a system reads a column its
query did not ask for) and row 5 (one `EcsTests` check through a `const World&`).

---

## 1. Chunks

Every archetype reserves `maxEntities` rows. Scene3 filled its 32 MB slab on wave 15, and 36 of its 41
archetypes were empty. The space prototype paid 3.65 MB for 4,064 reserved rows across 14
archetypes, 11 of them empty.

The roadmap row called these blocks. The plan uses chunk, the name Unity and Unreal Mass use.

A chunk is one fixed-size buffer that holds the same rows from every column of one archetype. The
archetype computes how many rows fit, once, from the chunk size and its row size. With an 8-byte
`Entity` and components of 12, 16 and 28 bytes, a row is 64 bytes, and a 16 KB chunk holds 256 rows:

| column | bytes per row | slice | offset |
|---|---|---|---|
| Entity | 8 | 256 × 8 = 2,048 | 0 |
| A | 12 | 256 × 12 = 3,072 | 2,048 |
| B | 16 | 256 × 16 = 4,096 | 5,120 |
| C | 28 | 256 × 28 = 7,168 | 9,216 |

Each offset is rounded up to its type's alignment, so a chunk can waste a few bytes at its end. An
archetype whose row is bigger than a chunk gets 0 rows and asserts.

### Decisions

1. **Chunks, not a growable column.** The World's arena never frees, so a growing vector leaves each
   old buffer behind, about twice the column's final size in dead bytes. Equal-size chunks can go back
   on a free list and any archetype can reuse them. The memory-manager step reserved rows for the same
   reason ([memory-manager.md](../dev-notes/memory-manager.md), "Columns reserve `maxEntities` rows").
2. **Each chunk holds every column for a range of rows,** not each column cut into its own chunks. One
   row count fits the whole chunk, so a row is a chunk plus an offset, and a chunk empties as one
   piece. Unity lays its chunks out this way: one array per component plus one of entity IDs.
3. **`EntitySlot` keeps one row number.** The World derives the chunk with `row / rowsPerChunk` and
   `row % rowsPerChunk`. It works because every chunk is full except the last one: a remove swaps the
   archetype's last row, which sits in the last chunk, into the gap. `EntitySlot`, `Locate` and the
   swap-remove fix-up stay as they are. The cost is a divide per `GetComponent`, which is not in a
   hot loop.
4. **The World owns a `ChunkPool`.** It takes chunk-size pieces through the `ArenaResource`, the same
   way the World takes memory for an `Archetype`, so the arena's counters see the chunks. When its
   free list is empty it takes a new piece; when an archetype hands a chunk back it keeps it. The
   World builds the arena, the resource and the pool in that order. Archetypes hold a pointer to the
   pool and own nothing else.
5. **Systems loop with `ForEachChunk`.** The query walks every matching archetype and chunk and calls
   the system's lambda once per chunk. The lambda is a template parameter, not a `std::function`, so
   it can be inlined. The query owns the loop, so it can add work around each chunk later (a
   `DebugWatch` check, a profiler span, threads) without a system body changing. `return` in the
   lambda moves on to the next chunk; no system breaks out of its loop today.
6. **An archetype gives a chunk back the moment it empties.** A spare chunk per archetype would bring
   back memory held by empty archetypes, and step 2 removes the in-between hops that would churn.
   Unity frees a chunk when its last entity leaves. Only the last chunk ever empties, and adds and
   removes inside a chunk never touch the pool.
7. **`WorldConfig` keeps both fields.** `maxEntities` now sizes only `entitySlots` and `freeSlots`,
   because those vectors also live in the arena and must not grow. `arenaBytes` covers the slots, the
   archetypes, their layouts and the chunks in use at the peak: a real total a scene can measure.
   `arenaBytes = 0` still works: the resource sends every request to the heap, and the pool still
   reuses chunks.
8. **The chunk size is a `WorldConfig` field, `chunkBytes`, default 16 KB.** `EcsBench` can then build
   Worlds at 4, 16 and 64 KB in one run of one binary. Each archetype reads it once to compute
   `rowsPerChunk`.

### The shape

```cpp
/** Where one component type sits inside every chunk of an archetype. */
struct ColumnLayout
{
    ComponentId type;
    uint32_t elementSize;
    /** Bytes from the start of the chunk to this column's first element. */
    uint32_t offset;
};

/** One fixed-size buffer that holds the same rows of every column of one archetype. */
struct Chunk
{
    std::byte* bytes;
    uint32_t count;
};

class Archetype
{
private:
    ComponentMask mask;
    /** Filled once at creation. The same for every chunk. */
    std::pmr::vector<ColumnLayout> columns;
    /** Every chunk is full except the last one. */
    std::pmr::vector<Chunk> chunks;
    uint32_t rowsPerChunk;
    ChunkPool* pool;
};
```

A system loop, using `PickSystem`'s box colliders:

```cpp
boxColliderEntities.ForEachChunk([&](const ChunkView chunk)
{
    const Transform* transforms = chunk.Column<Transform>();
    const PHYSICS::BoxCollider* colliders = chunk.Column<PHYSICS::BoxCollider>();
    for (uint32_t row = 0; row < chunk.Count(); ++row)
    {
        /* ... */
        pickedEntity = chunk.Entities()[row];
    }
});
```

`ChunkView` holds a pointer to the archetype and one to the chunk. `Column<T>()` uses the archetype's
layout to find the column inside the chunk, and `Archetype()` gives the archetype itself.

### What to measure

- **The baseline to beat:** under a nanosecond per entity at 100,000 rows in `EcsBench`, already
  bandwidth bound. Chunks do not make the loop faster; the columns are already packed today.
- **Per chunk size** (4, 16 and 64 KB): nanoseconds per entity, the cost of one migration, and chunk
  bytes per live entity.
- **Three new `FrameStats` counters:** `chunksTaken`, `chunksReturned` and `chunksFree`. Decision 6 is
  wrong if a scene with a steady entity count shows chunks taken and returned frame after frame. Then
  the fix is one spare chunk per archetype. Scene3's waves are the test.
- The spawn cost belongs after step 2, because the in-between hops each take and return a chunk.

### Build notes

- `Archetype::Column<T>()` without a chunk goes away, so every system moves to `ForEachChunk` in the
  same change, or the build breaks. `World::GetComponent` and the migration code switch to the
  row-to-chunk lookup at the same time.
- Each archetype that holds rows costs at least one chunk: Scene3's 41 archetypes cost at most
  41 × 16 KB = 656 KB.
- **For later shared values:** a shared value should be part of an archetype's identity, so cell 7
  and cell 8 become two archetypes. Shared values inside one archetype would give each value its own
  part-full last chunk, and decision 3's divide would stop working.
- Power-of-two rows per chunk, a chunk size per archetype and sparse-set storage are in ROADMAP.md,
  Hardening.

## 2. Create with components

Each spawn made about 9 migrations: 686,534 in 90 seconds in Scene3, and `Playback` was the most
expensive system. The space prototype paid 8 per entity through plain `AddComponent`, so the fix
belongs in the World, not only in playback.

The idea, as a sketch:

```cpp
const Entity rock = world.CreateEntity(
    Transform{ .position = { 0.0f, 5.0f, 0.0f } },
    RigidBody{ .mass = 2.0f },
    MeshRef{ cubeMesh });
```

The World finds the final archetype from all the types at once and copies each value's bytes. No
constructor runs inside the World: the caller's code builds each value, so `Transform{}` already
holds scale 1 and the identity quaternion when the World sees it. The command buffer gets the same
call, because systems spawn through it. `AddComponent` and `RemoveComponent` stay as they are, for
gameplay changes such as adding `Picked` on a grab.

Bevy spawns a tuple of components in one call, and its required components fill missing ones from
`Default`. Unity creates an entity straight from an archetype.

### Decisions

1. **No default bytes in this step.** No path here leaves a column without a value: the typed call
   takes one per component, tags have no column, and the command buffer copies every value when it
   records. No caller, no code. The first caller is File I/O, where a saved entity can miss a field;
   networking has the same need. Adding them then is pure addition: a `defaultBytes` pointer in
   `ComponentInfo`, filled in `Register<T>()` from a `static const T defaultValue{}`, and one branch
   in `CreateEntityById` that copies it for a column with no value.
2. **A typed `CreateEntity(values...)` wraps an untyped `CreateEntityById`,** the same pattern as
   `AddComponent` and `AddComponentById`. Only the untyped call places the row, so the typed call,
   playback, and File I/O later share one path. The command buffer needs the untyped call because
   its payload holds ids and bytes, not types.
3. **The command buffer records one `CreateEntityWith` command** holding every component of the new
   entity. The payload stores id, size and bytes per component, as offsets, because the payload
   vector can move as it grows. Playback builds a `ComponentValue` list with pointers into the payload
   and calls `CreateEntityById`. The placeholder works as `CreateEntity()`'s does today. Rejected:
   keeping `CreateEntity` plus `AddComponent` commands and merging them at playback. The merge breaks
   when a system interleaves commands (create A, create B, add to A), and it hides the cost instead
   of fixing the call.
4. **The same type passed twice fails.** A `static_assert` in the typed calls of the World and the
   command buffer catches it at compile time. A debug `assert` in `CreateEntityById` catches a
   repeated id while the mask is built, for playback and File I/O.

### The shape

```cpp
/** One component's id and bytes, for paths that have no type in hand. A tag passes nullptr and 0. */
struct ComponentValue
{
    ComponentId id;
    const std::byte* bytes;
    uint32_t size;
};

/** Makes an entity straight in the archetype of all the given components. */
template <typename... Ts>
Entity CreateEntity(const Ts&... values)
{
    static_assert(kAllDifferent<Ts...>, "CreateEntity got the same component type twice.");
    const ComponentValue list[] = { ValueOf(values)... };
    return CreateEntityById(list);
}

/** Makes an entity straight in the archetype of every listed component, copying each value in. */
Entity CreateEntityById(std::span<const ComponentValue> values);
```

`CreateEntityById` ORs the ids into a mask, finds or makes the archetype with `ArchetypeIndexFor`,
takes a slot, adds one row and copies each value into its column. It counts 1 in `entitiesCreated` and
one per component in `componentsAdded`, and zero migrations. Today's `CreateEntity()` with no
arguments stays; C++ prefers it over the template with an empty pack.

Playback:

```cpp
case CommandKind::CreateEntityWith:
{
    ComponentValue list[kMaxComponents];
    const uint32_t count = ReadComponents(payloadOffset, payloadSize, list);
    created.push_back(world.CreateEntityById({ list, count }));
    break;
}
```

### Build notes

- The new call only helps where code uses it. The spawn sites in the scenes and systems switch from
  `CreateEntity()` plus `AddComponent` calls to the new call, and the migration counter shows the
  drop.
- The spawn cost from step 1's measurements belongs here: each in-between hop also took and returned
  a chunk.

## 3. Clear the World

A restart destroys every entity one at a time. One call should empty every archetype and reset the
entity slots. With chunks, this means handing every chunk back to the pool.

Bevy's `World` has `clear_entities`. From memory, EnTT has `registry.clear()`.

**Where the need came from.** On `main`, nothing destroys entities one by one: F3 in `PhysicsScene`
goes through `SceneManager`, which destroys the whole scene and its World and builds new ones. The
row came from the prototype games, which restart a round inside a running scene: Scene2's
`RestartRound` and Scene3's R key (findings/2026-10-prototype-game.md, findings 8 in both sections).
They could not ask `SceneManager` for a rebuild, because a scene cannot reach it. A clear keeps what
lives outside the World (score, settings, camera), the systems and their cached queries, and the
World's slab.

Later callers: loading a save (File I/O: clear, then `CreateEntityById` per saved entity), a network
resync (Networking: the server sends the full state), `EcsBench` reusing one World between cases, and
an editor's play mode restoring the level, which builds on loading a save.

### Decisions

1. **Keep the step, with `PhysicsScene`'s restart as the caller.** Restarting a round is the normal
   case in a game, the prototypes needed it twice, and File I/O becomes the real caller later.
2. **`Clear` keeps the slot table, bumps the version of every live slot and marks it free.** A handle
   held from before must read as dead. Emptying the slot vector would restart versions at 1, so an
   old `{index 3, version 1}` would match the first new entity at index 3. With step 4, marking free
   becomes the same two-line link as in `DestroyEntity`.
3. **`Clear` keeps every archetype.** Each one hands its chunks back to the pool and drops to 0 rows;
   its mask and layout stay, and `archetypeGeneration` does not change. A restart spawns the same
   kinds of entities again. Destroying archetypes would leave dead copies in the arena, which cannot
   free, so the slab would fill a little more every round, and every cached query would search
   again. The respawn reuses the chunks from the pool's free list.
4. **`Clear` is a direct World call, made only from scene code outside the phases. It is not a
   command.** Outside the phases no system is mid-loop, and the command buffer is empty, because
   `RunPhase` plays it back at the end of every phase. Inside a system, a loop would keep a pointer
   into a chunk that went back to the pool. As a command, a clear would run between other commands,
   and a bullet queued before it would appear in the fresh world. No debug check: it would need the
   World to see the registry's running flag.
5. **`PhysicsScene` gets `SpawnShapes()`.** `OnCreate` keeps the asset loads and every
   `systems.Add`, then calls `SpawnShapes()`, which makes the two cubes, the four Marios and the
   `PickRay` entity. R in `PhysicsScene::Update`, next to the C and M toggles, calls `world.Clear()`
   and then `SpawnShapes()`. Not F3: F1 to F4 belong to `Application` for switching scenes, and F3's
   rebuild stays, so the two paths can be compared. Both prototypes used R.

```cpp
void World::Clear()
{
    for (Archetype* archetype : archetypes)
    {
        archetype->ReleaseChunks();
    }

    for (uint32_t index = 0; index < entitySlots.size(); ++index)
    {
        EntitySlot& slot = entitySlots[index];
        if (slot.archetype == kInvalidArchetype) continue;
        slot.version++;
        slot.archetype = kInvalidArchetype;
        freeSlots.push_back(index);
    }
}
```

### Build notes

- Step 2 lands first, so `SpawnShapes()` uses `CreateEntity(values...)`: one call per shape instead of
  seven.
- In the counters, `chunksReturned` jumps on the clear frame and `chunksTaken` matches it on the
  respawn, with the arena's used bytes unchanged.
- A test: hold a handle, clear, and check that `DoesEntityExist` is false and that the next entity
  at that index has a higher version.

## 4. An intrusive free list

A dead slot already has `archetype = kInvalidArchetype`, and its `row` holds nothing useful. The free
list stores the next free index in that field, and the World keeps one `firstFree` index instead of
the `freeSlots` vector.

```cpp
/* DestroyEntity: the dead slot points at the old first free slot. */
slot.archetype = kInvalidArchetype;
slot.row = firstFree;
firstFree = entity.index;

/* CreateEntity: take the first free slot, and move firstFree to the next one. */
index = firstFree;
firstFree = entitySlots[index].row;
```

`DoesEntityExist` already tests `archetype != kInvalidArchetype`, so it does not change. The reuse
order stays the same: the slot freed last comes back first. The risk: `row` now means two things, so
code that reads `row` without checking `archetype` first would read a free index as a row.

The gain is small, one vector less. It is in this plan because it touches the same code as step 3.
EnTT uses the same trick (skypjack, ECS back and forth, part 3). The row is not from the findings;
git first shows it in commit `9f4bc97`.

### Decisions

1. **The World keeps one `uint32_t firstFree`; a dead slot's `row` holds the next dead slot's index,
   and `kNoFreeSlot = 0xFFFFFFFF` ends the list.** It keeps today's order, so every existing slot test
   should pass unchanged and serves as the proof. No real index can reach `0xFFFFFFFF`. Threading
   changes nothing: structural changes run on one thread, and the Concurrency topic keeps playback
   sequential. The vector is no safer, since `push_back` is not thread-safe either. A lock-free list
   would face the ABA problem; a plain lock avoids it.
2. **`maxEntities` reserves only `entitySlots`.** The `freeSlots.reserve` line goes with the vector,
   and step 1's `WorldConfig` comment loses "and `freeSlots`". The saving is `maxEntities × 4` bytes
   of arena: 16 KB at 4,096.
3. **An `assert` in `CreateEntity` and a `CheckFreeList()` for the tests guard the double meaning of
   `row`.** The real risk is a stray write to a dead slot's `row`, which would break the chain with no
   error, where the vector would not care. The assert catches a broken link at the point of use.
   `CheckFreeList()`, debug only, walks the list and checks that every linked slot is free, that the
   length equals the number of free slots, and that the walk ends within `entitySlots.size()` steps.
   `EcsTests` calls it after every create, destroy and clear sequence. Today only the swap-remove
   fix-up in `DestroyEntity` writes `row` on purpose, and it writes a live slot.

```cpp
/** Ends the free list. No real slot index reaches it, so it can never collide. */
constexpr uint32_t kNoFreeSlot = 0xFFFFFFFF;

/* DestroyEntity and Clear: link the dead slot in front. */
slot.archetype = kInvalidArchetype;
slot.row = firstFree;
firstFree = index;

/* CreateEntity: unlink the front slot. */
if (firstFree != kNoFreeSlot)
{
    index = firstFree;
    assert(entitySlots[index].archetype == kInvalidArchetype && "the free list points at a live slot");
    assert((entitySlots[index].row == kNoFreeSlot || entitySlots[index].row < entitySlots.size())
           && "the free list links past the slot table");
    firstFree = entitySlots[index].row;
}
```

### Build notes

- Step 3's `Clear` sketch switches its `freeSlots.push_back(index)` to the same link as
  `DestroyEntity`.
- The memory-manager dev-note and `docs/tools/arena.md` name the free list as a vector; check them
  at commit time.

## 5. Entity breakpoints

A data breakpoint cannot follow an entity: adding or removing a component moves its bytes with a
memcpy, and the breakpoint keeps watching the old address (ECS.md house rule 11). A conditional
breakpoint on the entity index works, but you must find the index first, and the debugger tests the
condition on every row of every frame, so the scene crawls.

**The case.** Cube 2 jitters after a release. You add `DebugWatch` to cube 2 in the code and run
under Rider. Every system that reaches cube 2 stops, and only for cube 2. The tag moves with the cube
through every migration, because it is part of the entity's archetype.

**A change from the roadmap row.** The row asked for a flag per block (a chunk, in this plan). A
`DebugWatch` tag already puts the entity in its own archetype, so the archetype's mask answers the
question, once per archetype. A per-chunk flag only helps if watching becomes a bit that does not
move the entity (roadmap row 10). So this step does not need chunks. With step 1's `ForEachChunk`,
the check can live inside the query, so no system writes it by hand.

Unreal Mass has the same idea: select an entity, then break when a processor reaches it, or when
code writes one of its fragments. Its breakpoints fire only in loops that use Mass's own entity
iterator. Unity has inspection and Journaling, a record of what happened to each entity, but no
breakpoints.

### Decisions

1. **The check lives inside `ForEachChunk`.** The query tests each matching archetype's mask for
   `DebugWatch` once, then calls `DEBUG::BreakOnWatch()` before it hands each chunk of a watched
   archetype to the system's lambda. No system writes the check, and no system needs `DebugWatch` in
   its query: a query matches any archetype that has its components, extra ones included, and the
   check reads the archetype's mask, not the query's. Adding the tag makes a new archetype, which
   bumps the generation, so every cached query picks it up. After the stop, F11 steps into the
   system's row loop; with one entity watched, its archetype holds one row, and row 0 is the entity.
2. **The break is armed by default, and Rider turns it off and on.** `DEBUG::watchBreakArmed` starts
   `true`. Set it to `false` in Rider's watch window to let the game run past the watched entity, and
   back to `true` for the next stop. Mass works the same way: its breakpoint is cleared by setting
   `bDisableThisBreakpoint` in the IDE's watch window. A global flag, because the World store
   (roadmap row 5) does not exist yet and the flag belongs to the debugger, not the game. Rejected: a
   key that arms the break for one frame. It needed a system to read the key, and a frame with no
   Fixed step would skip the Fixed systems.
3. **Without a debugger attached, nothing happens.** `BreakOnWatch` returns unless
   `IsDebuggerPresent()` is true. That Windows function answers whether any debugger (Rider, Visual
   Studio, WinDbg) is attached, whatever the build type, so the guard also works in Release under
   Rider. `__debugbreak()` with no debugger would raise an exception nothing handles, and Windows
   would close the game. Rejected: printing a line per system instead. No log is needed now.
4. **No tagging system. The developer adds the tag in their own code while debugging, and removes
   it afterwards.** A system that tagged the held shape would depend on picking and be useless in a
   scene without it. The tag can go in at creation, or at the moment a bug appears, through the
   command buffer.

```cpp
/* core/debug: the flag and the break. In a .cpp, so <windows.h> stays out of every header. */
namespace DEBUG
{
    /** Set to false in Rider's watch window to let the game run past watched entities. */
    inline bool watchBreakArmed = true;
}

void DEBUG::BreakOnWatch()
{
    if (!watchBreakArmed || !IsDebuggerPresent()) return;
    __debugbreak();
}

/* Query: the check, once per archetype. */
template <typename F>
void Query::ForEachChunk(F&& visit)
{
    for (Archetype* archetype : Matches())
    {
        const bool watched = (archetype->Mask() & COMPONENTS::MaskOf<DebugWatch>()) != 0;
        for (Chunk& chunk : archetype->Chunks())
        {
            if (watched) DEBUG::BreakOnWatch();
            visit(ChunkView{ archetype, &chunk });
        }
    }
}

/* Game code, while debugging: at creation, or when the bug appears. */
world.AddComponent<DebugWatch>(cube2);
context.commands.AddComponent<DebugWatch>(owners[row]);
```

### Cost

The check runs once per archetype per query, not once per entity: 10,000 entities in one archetype
pay one mask test, and the row loop holds no check. With no tag in the code, `BreakOnWatch` never
runs, so `IsDebuggerPresent` never runs either. From memory, it reads one flag in the process's own
memory. Release keeps all of it; a shipping build would put it behind the define from the roadmap's
"A shipping build" row.

### Build notes

- `DebugWatch` lives in `ecs/`, because `Query` reads it and `ecs` must not include `physics` or
  `core`. `DEBUG::BreakOnWatch` and the flag live in `core/debug`, next to the line helpers; check
  that `ecs` may call into `core/debug`, or move the two into `ecs/`.
- `FixedStep` caps the catch-up time (`kMaxAccumulated`), so physics should not leap after a long
  stop. Vulkan and SDL after a pause need a real run.

Checks for the build:

| # | check | how |
|---|---|---|
| 1 | The break hits only the watched entity | Tag cube 2 in `SpawnShapes`, run under Rider, stop in `ApplyForcesSystem`, read row 0's entity |
| 2 | The tag survives grab and release | Unit test: tag, add `Picked`, remove `Picked`, check the tag |
| 3 | Turning the flag off lets the game run | Set `DEBUG::watchBreakArmed` to `false` in Rider's watch window, continue |
| 4 | No crash without a debugger | Run the plain exe with cube 2 tagged |
| 5 | The game recovers after a long pause | Wait a minute in the debugger, then continue |
| 6 | The check costs nothing with no tag | `EcsBench` at 100,000 rows, with and without the check |
