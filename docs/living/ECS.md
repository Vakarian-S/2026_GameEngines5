# ECS

**Last updated:** 2026-11-05.

The ECS design: the settled decisions, the invariants, the rules for systems and phases, and the
house rules. The data model lives on the Miro board, under "The diagram".

## Contents

- [Decisions that are settled](#decisions-that-are-settled)
- [Invariants worth protecting](#invariants-worth-protecting)
- [Structural changes during iteration](#structural-changes-during-iteration)
- [Systems and phases](#systems-and-phases)
- [The diagram](#the-diagram)
- [Appendix — the house rules](#appendix--the-house-rules)

---

## Decisions that are settled

Do not reopen these without a measurement.

| decision | reason |
|---|---|
| **Archetype storage**, not sparse set | the house rules assume it — tags only remove work from a loop when they physically partition memory |
| **Systems loop with `Query::ForEachChunk`** | the query walks every matching archetype and chunk, and calls the system's lambda once per chunk with a `ChunkView`. The query owns the loop, so work around each chunk (a profiler span, a debug check, threads) goes in one place. The lambda is a template parameter, so it can be inlined |
| **Single threaded** | at a few thousand entities, iteration is not the bottleneck. If threads ever come, a scheduler may split one system's rows across threads, or run neighbouring systems side by side when they do not conflict. It never reorders them. See "Systems and phases" |
| **`pmr::vector<Archetype*>`, each archetype built in the arena with placement new** | the vector reallocates as archetypes appear, and queries cache raw `Archetype*`, so the archetypes must not move. `World`'s destructor destroys each one by hand |
| **Components are trivially copyable** | enforced by `static_assert` in `Register<T>`. It is what makes an archetype move a `memcpy` and nothing else. It is not an ECS law: EnTT needs none of it, because nothing there migrates |
| **Components need no more alignment than plain `new`** | also a `static_assert`. A chunk starts aligned like plain `new`, and the archetype rounds each column's offset up to its type's alignment. `sizeof(T)` being a multiple of `alignof(T)` then keeps every element aligned. 16 on x64, 8 on Win32 |
| **Archetypes store rows in fixed-size chunks from the World's `ChunkPool`** | the arena never frees, so a growing column would leave its old blocks behind. Equal-size chunks go back on the pool's free list when an archetype empties one, and any archetype can reuse them. Measured 2026-11-04 with `EcsBench` at 100,000 entities: the arena peak fell from 10,548 KB, when every archetype reserved `maxEntities` rows, to 4,759 KB. `WorldConfig::chunkBytes` sets the size, 16 KB by default |
| **Each chunk holds every column for a range of rows** | the entity IDs sit at offset 0, then one slice per component. One row count fits the whole chunk, so a row is a chunk plus an offset, and a chunk empties as one piece. Unity lays its chunks out the same way |
| **Every chunk is full except the last one** | a remove swaps the archetype's last row into the gap. So an `EntitySlot` keeps one row number, and the archetype finds the chunk with `row / rowsPerChunk` and `row % rowsPerChunk` |
| **`World` owns its arena. `arenaBytes = 0` is heap mode** | one program can compare both modes, and the heap path still goes through the program's `operator new`, so the Memory Monitor counts it |
| **Tags have size 0** | from `std::is_empty_v<T>`, not `sizeof`, which is 1 for an empty struct |
| **`GetComponent<T>` rejects tags at compile time** | a tag has no data to point at; `HasComponent<T>` tests the mask instead |
| **Archetypes are never destroyed** | slots hold indices into the vector and queries hold pointers into it |
| **The command buffer records into its own memory** | recording never reads or writes the world, which is what makes one buffer per thread possible later with no locking |
| **Placeholders are a flag bit on the index**, DOTS-style | an index above 2^31 is past the end of any slot table, so every existing `World` check already answers correctly for one, with no new code |
| **Playback applies in recording order**, no coalescing | grouping an entity's operations into one move is a real win with sharp edges. The counters decided it on 2026-09-30: not needed. 24,000 migrations fit inside a 4.2 ms frame |
| **`AddComponentById` is public, not friended** | playback holds a `ComponentId` and loose bytes, never a type. One honest public method beats making `CommandBuffer` a friend of `World` |

**Not taken, from the EnTT comparison (2026-09-28):** sparse-set storage, views and groups,
`entt::meta` reflection, construct and destroy signals (rule 7 says scan), header-only templates,
and packing `Entity` into one 32-bit integer.

---

## Invariants worth protecting

- **Versions start at 1.** A default `Entity{}` is `{0, 0}` and must never match a live slot.
- **The version bump happens only in `DestroyEntity`.** Bump it anywhere else and handles stop
  matching the slots they name.
- **One source of truth for a row.** Nothing anywhere stores a row except the entity's slot.
  `RemoveRow` returns the entity that moved so exactly one slot gets patched.
- **Adding or removing a component never touches the version.** Same entity, different shape.
- **`archetypeGeneration` is bumped on the create path only.** Bumping it on lookup would make
  every query rebuild forever.
- **Matching is by mask, never by contents.** An archetype with zero rows still matches. This is
  why entities moving between archetypes cannot invalidate a query.
- **Column pointers die on any structural change.** `ChunkView::Column<T>()`, `Entities()`,
  `GetComponent<T>()` and `Matches()` all return things that go stale on the next `AddRow`,
  `RemoveRow`, `AddComponent` or `DestroyEntity`. `AddRow` never moves an existing chunk, but a
  remove shifts rows, and an emptied chunk goes back to the pool, where another archetype reuses it.
- **A `ChunkView` lives only inside the `ForEachChunk` call that hands it out.** It owns nothing.
  Never store one, or a pointer it returned.
- **An archetype gives a chunk back the moment it empties, and its destructor gives back the
  rest.** So every chunk is on the pool's free list when the pool dies, and the pool frees them all.
  This keeps heap mode at 0 leaked blocks.
- **`Clear()` resets `placeholderCount` alongside `created`.** If the counter kept climbing
  while the list restarted, `Resolve` would read the wrong slot and say nothing.
- **`Clear()` does not reset `playedInto`.** Every other member belongs to one round; that one
  belongs to the buffer's whole life, and resetting it turns the wrong-world assert off.
- **A placeholder is a command target, never a value.** Component bytes are copied when the
  command is recorded and playback never looks inside them, so a placeholder stored in a
  component holds a fake handle forever.
- **A placeholder dies with the pass that made it.** After a playback its number means whatever
  the next round hands out. Outside a loop, call `World::CreateEntity` and get a real handle.
- **`Playback` clears the buffer.** Replaying one twice would apply handles the first pass
  invalidated, so it is made impossible rather than written down.
- **The arena members of `World` are declared before every container that uses them, and the
  `ChunkPool` after them.** Members are destroyed in reverse order. A container that outlived its
  resource would give its memory back to a dead object, and the pool must outlive the archetypes
  that give their chunks back to it.
- **`CommandBuffer`, `Query` and the scene's own vectors stay on the heap.** They keep their blocks
  across frames, so a frame arena reset would leave them pointing at reused memory.

---

## Structural changes during iteration

A system may not create, destroy, add or remove while walking a block. It has two ways to go
wrong, and they fail very differently.

**Shuffle** — destroy, add or remove. `RemoveRow` slides the archetype's last row into the one
the loop is standing on, and the last row may sit in another chunk. The loop skips an entity and
ends early. If the last chunk empties, it goes back to the pool, and the loop can then read a
chunk that another archetype reuses.

**Growth** — create. `AddRow` may push a new chunk onto the archetype's chunk list, and that
vector can reallocate, so the loop's iterator dangles. The last chunk's `Count()` also grows, so a
system that spawns into the archetype it reads from can run without end.

The fix for both is the same: record the change, keep looping, apply at a sync point.

```cpp
query.ForEachChunk([&](const ChunkView chunk)
{
    const Health* health = chunk.Column<Health>();
    const Entity* rows = chunk.Entities();

    for (uint32_t i = 0; i < chunk.Count(); ++i)
    {
        if (health[i].current <= 0.0f) buffer.AddComponent<Dead>(rows[i]);
    }
});

buffer.Playback(world);
```

Recording touches nothing outside the buffer, so `health` stays valid and `Count()` is fixed
for the whole pass. That property is also what makes one buffer per thread possible later.

**Only shape changes need the buffer.** Writing a component's value moves nothing and resizes
nothing, so it goes straight to memory, always.

The only remaining rule is about placeholders, in "Invariants worth protecting".

---

## Systems and phases

Built in step 3; the history and measurements are in dev-notes/systems-and-phases.md.

**Phases.** `Phase` runs Fixed, then Update, then Extract. Fixed runs 0 or more times per frame,
1/60 s each. Update and Extract run once per frame. Extract reads the world and fills the draw
lists, and writes no component.

- A phase exists only for a different clock, as Fixed has against Update, or for a sync point: a
  system needs another system's queued changes in the world first. Never add one for a lifecycle
  or a feature.
- Inside a phase, systems run in the order the scene adds them. Nothing sorts them.
- Two writers of one component may share a phase, and the add order decides who goes first.
  `SystemRegistry::Print` notes each such pair at startup and fails nothing.
- Read key and mouse presses in Update, never in Fixed. Fixed runs 0, 1 or 2 times per frame, so a
  press read there is missed or counted twice. Held keys and buttons are safe in any phase.

**Systems.**

- A system is a class that derives from `System`. It owns its query and its state, and has one
  function, `Run`.
- One instance per scene. The scene creates it in `OnCreate`, and it dies with the scene. A system
  built every frame would rebuild its query cache and reset its counters.
- The name, reads and writes describe the system's code, so the `System` constructor takes them,
  and nothing changes them later. The phase is a scheduling choice, so the scene gives it in
  `SystemRegistry::Add`.
- The name is a string literal, because the profiler keeps it by pointer. Code never looks a
  system up by name.
- A structural change through the command buffer counts as a write.

**The registry owns the command buffer.** `RunPhase` plays it back after each phase and after each
fixed step, so no caller can forget a playback. The buffer handles only create, destroy, add and
remove. A value write goes straight into the column, so the add order decides which of two value
writes wins. "The playback rule" below says what a system sees.

**Three habits keep threads possible.** They cost nothing on one thread.

1. A system reaches the world, the buffer and the lists only through `SystemContext`, never
   through the scene.
2. A system keeps no shared state across rows, such as a static counter.
3. The order stays manual. A later scheduler may run neighbours side by side when they do not
   conflict, but never reorders them.

**The const `Column`.** `ChunkView::Column<T>() const` returns `const T*`. An Extract system's
lambda takes a `const ChunkView`, so the compiler stops a write. It does not check a system's
declared reads against its code.

**The chunk counters.** `FrameStats` counts `chunksTaken` and `chunksReturned`, which are events,
so they reset every frame like `migrations`. The pool writes them through a reference to the
World's `FrameStats`. `WorldStats::chunksFree` is a level, so `World::Snapshot` reads it from the
pool.

**The profiler.** `RunPhase` opens one span per system, named after it. A playback gets an ECS
span named "Playback Fixed", "Playback Update" or "Playback Extract". An empty buffer gets no
span, so the table shows how often each phase really plays back. Two instances of one class share
a name, and the profiler adds their times.

---

## The diagram

The Miro board holds the data model: https://miro.com/app/board/uXjVHizgbEY=/

---

## Appendix — the house rules

Vakarian's design rules, taken from a talk by an Epic engineer on Unreal's Mass framework.
They are the style guide for this engine and they drove most of "Decisions that are settled". Where one does not
fit, say so and why rather than following it.

1. Judge designs by memory waste: how much of each cache line read is actually used. Chasing
   pointers to reach a few bytes is the thing to avoid.
2. An entity is an ID plus a version. Nothing else. No base class, no methods.
3. Components are plain data. Split them by how they are accessed, not by concept. If a system
   reads one field out of a fat component, the component is too fat.
4. Systems are small. A system that writes more than one component type needs a second look.
5. Every system declares what it reads and what it writes, even on one thread. That gives
   ordering, conflict checks and a debug view.
6. Use tags (empty components) to move branches out of hot loops. Composition decides what
   runs, not if-statements inside the loop.
7. Prefer scanning every frame over callbacks and events. A system scans health and adds a
   `Dead` tag, instead of an `OnDeath` delegate.
8. Entities are not game objects. Use short-lived entities as messages, payloads and
   reservations, and destroy them at the end of the frame.
9. Structural changes (create, destroy, add, remove) are queued in a command buffer and applied
   at explicit sync points, never mid-iteration.
10. Hierarchies work by storing depth and processing shallow depths first, not by walking
    parent pointers.
11. Plan for debugging from the start: data moves, so normal data breakpoints are useless.
    Query-based inspection, and a way to break when a chosen entity is about to be written.
12. Design in diagrams first. The code will not look like the design, so the data flow diagram
    is the real document.

### Mass terminology

The talk's vocabulary, mapped onto this engine. Useful when reading Mass material or
watching the talk again.

| Mass | here |
|---|---|
| Processor | System |
| Fragment | component |
| Tag | tag |
| Archetype | Archetype |
| EntityQuery | `Query` |
| `FMassEntityHandle { Index, SerialNumber }` | `Entity { index, version }` |
| `Entity.AddTag()` | `commands.AddTag<T>(entity)` — entities have no methods here |

### Where these were qualified

- **Rule 3** as written implies fat components are bad. With struct-of-arrays columns, size
  alone costs nothing — a system that never reads a component does not pay for it. The useful
  form is: split a component when a hot system touches only part of it.
- **Rule 4** is a smell, not a law. A physics integrator writing both position and velocity in
  one pass is correct; splitting it would read both arrays twice.
- **Rule 7** holds everywhere inside the world. It breaks at the boundary to a *retained*
  rendering API, where a handle must be created and destroyed exactly once. Confine that
  change-detection to the render system rather than reintroducing delegates.
- **Rule 8** has a price: every message entity is a structural change. Use one when several
  systems want to query and filter it independently; use a plain per-frame array when there is
  exactly one reader.
- **Rule 1** is the right lens, but at a few thousand entities the hot data is small enough to
  sit in cache. Expect a modest win here, not the numbers from a million-entity demo.

### Rules of this engine

These rules come from this engine's own code, not from the Mass talk.

**Vocabulary.** *Destroyed* or *gone* means the entity no longer exists — `DoesEntityExist`
is false and the slot is free. *Dead* means the gameplay `Dead` tag on an entity that still
exists. Never say "died" for destruction. Conflating these caused confusion twice.

**The zero-bytes rule.** Added 2026-10-27, in step 4.0: dev-notes/transform.md.

- `Archetype::AddRow` fills a new row with zero bytes. It never runs a constructor.
- So every add of a component with data must hand over that component's bytes. `T{}` is the
  default value. `World::AddComponent<T>` and `CommandBuffer::AddComponent<T>` both copy it when
  the caller passes nothing.
- A component whose zero bytes are not a valid value depends on this rule. A zeroed `Transform`
  has scale 0 and the zero quaternion, so the mesh draws as a single point and no error appears.
- `World::AddComponentById` asserts it. A component whose size is not 0 must come with a value
  and a size. A tag has size 0, so it passes with `nullptr`.
- Two future paths will need the same care: loading entities from a file, and ROADMAP.md's
  create-with-components. A default value per component type waits for the second one (ROADMAP.md,
  ECS hardening, row 2).

**The playback rule.** Added 2026-10-28, in step 4b: dev-notes/physics-scene.md.

- A component added or removed through the command buffer is not in the world until playback.
- `SystemRegistry::RunPhase` plays back at the end of each phase. Systems in later phases see
  the change; systems in the same phase never do.
- `PickSystem` queues `Picked` in Update, so `DrawColliders` draws the yellow wire in the same
  frame's Extract.
