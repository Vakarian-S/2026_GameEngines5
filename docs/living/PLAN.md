# Plan

**Last updated:** 2026-11-18.

This plan builds a store on the World for world-unique data, keyed by type, which systems read
through `SystemContext`. Bevy calls these resources. It serves two goals: `ecs` stops naming outside
types such as `DrawItem` and `Camera`, and the networking plan gets a place for its per-World state.
It comes from ROADMAP.md, ECS, Hardening row 5. Hardening row 6 (run conditions) and Networking row 1
(the Boss Fight Scene) both need it.

## Steps

| # | step | done when | stage |
|---|---|---|---|
| 1 | Rename the asset side: `ResourceStore` and `ResourceLoader` become `AssetStore` and `AssetLoader` | No `ResourceStore` or `ResourceLoader` is left outside dev-notes and findings, and every scene runs as before | designed |
| 2 | The store, called `Resources`: one value per type on the World, read and written through `SystemContext` | A test puts a value in a World's store, a system reads it, and a second World holds its own value | designed |
| 3 | Move `SystemContext`'s one-per-world fields into the store: `deltaTime`, the draw list, the debug lines, the input state and the camera | `SystemContext.h` names no type outside `ecs` and the standard library, and every scene runs as before | designed |

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

---

## 1. Rename the asset side

**Why.** Decided 2026-11-18. The ECS store is called `Resources`, to match Bevy. The engine already
uses "resource" for meshes, textures, materials and cube maps. Two meanings of one word would clutter
every grep, so the asset side gets its own word first.

**Decisions.**

- **a. Type names.** `ResourceStore` becomes `AssetStore`, and `ResourceLoader` becomes `AssetLoader`.
  The name says what the class is. Plain `Assets` was rejected: Bevy's `Assets<T>` holds one asset
  type, and ours holds four.
- **b. Variables and getters.** `resourceStore` becomes `assetStore`, `resourceLoader` becomes
  `assetLoader`, and `GetResourceStore` becomes `GetAssetStore`. A rename of the types alone leaves
  the old word in the code.
- **c. Files.** `ResourceStore.h` and `.cpp` and `ResourceLoader.h` take the new names. The folders
  stay (`vulkan` and `core`). `git mv` keeps the history. `CMakeLists.txt` needs no edit, because it collects `src` and `include` with a glob.
- **d. What stays.** `ArenaResource` and `std::pmr::memory_resource` keep their names.
  `memory_resource` is the standard library's word for an allocator hook. A grep for `Resource` still
  hits them, so grep `\bResources\b` for the new class.
- **e. Docs.** Update the living docs only: STATE.md, ROADMAP.md, PLAN.md and ECS.md. Dev-notes and
  findings keep the old names, because dev-notes are legacy and never patched.
- **f. Check.** Rider's Rename (Shift+F6) on each class and variable. Then build, run
  `scripts\run-tests.bat` in all four configurations, and run each scene with
  `scripts\run-game.ps1`. The error stream must be clean, with 0 leaked blocks.

## 2. The store

**Why a separate store.** Decided 2026-11-18, while planning the networking scene. Resources must
hold types that rule 4 forbids in a component: a socket, a client table in a vector, a map from
network ID to entity. So they live in a store on the World, keyed by type, and not on singleton
entities. EnTT does the same with `registry.ctx()`, Unreal Mass with world subsystems, and Bevy did
until 0.19. Bevy 0.19 moved resources onto entities so hooks, observers and relations would work on
them without a second copy of each feature (PR #20934). Rust moves every type with a byte copy, so
Bevy needs no rule 4. This engine has none of those features yet, so the store duplicates nothing
today. Revisit when an observer, a hook, an entity inspector or saving starts needing its own
version for resources: then weigh ROADMAP "Worth weighing" row 1 and resources as entities.

**Rules for the store.** Systems reach resources only through `Get<T>()` on `SystemContext`, never
the store itself, so moving resources onto entities later touches one place. A resource lives per
World, never in a static: a listen server runs a server World and a client World in one process.
Overwatch hit this when Killcam added a second simulation and its globals broke (GDC 2017,
"Overwatch Gameplay Architecture and Netcode", 11:05 to 12:24). Overwatch then moved most system
state into singleton components, about 40% of all its components (13:36).

**Decisions.** Agreed 2026-11-18.

- **a. Ids and mask.** A new `include/ecs/Resource.h` holds `ResourceId`, `ResourceMask` (a
  `uint64_t`), `kMaxResources = 64` and `RESOURCES::IdOf<T>()`. The counter is separate from the
  component counter, so component ids stay free. Sharing the counter was rejected.
- **b. Storage.** `Resources` holds 64 slots and the mask. Each slot has a pointer to its value and
  a destroy function. Each value is built in its own allocation from the World's `memory`, so
  references stay valid and the leak check sees the memory. One byte buffer with offsets was
  rejected, because it moves when it grows. `Resources` is declared after `memory` in `World`, so
  it is destroyed first.
- **c. API.** `Insert<T>(args...)` builds the value in place and returns `T&`. `Get<T>()` returns
  `T&` and asserts the value exists. `TryGet<T>()` returns a pointer, or null. `Has<T>()` tests the
  mask bit. A second `Insert` of the same type asserts. There is no `Remove<T>()`, because nothing
  calls it yet. `Get` returning a pointer was rejected. `Set<T>(const T&)` inserts when the type is
  absent and assigns when it is present, at the same address. It exists for values the engine
  refreshes every frame, and was added 2026-11-19, once step 3 had five callers. `Insert` keeps its
  constructor arguments for types that cannot be copied, such as the sockets the networking plan lists.
- **d. When to insert.** `Insert` is a direct call during scene setup, not a command. Rule 6
  protects entity rows being walked, and an insert moves none. The command buffer was rejected,
  because nothing needs a mid-phase insert.
- **e. Constness.** `SystemContext::Get<T>()` always returns `T&`. There is one thread, and no
  caller wants a read-only view. Declared read and write access waits for the threading work. A
  `Res`/`ResMut` split now was rejected.
- **f. `World::Clear`.** It leaves resources alone, so a respawn does not drop a socket. A system
  resets its own resource when it wants to. Clearing them was rejected.
- **g. Teardown order.** The World destroys resources in reverse insertion order, tracked in a small
  array of ids, because a later resource may point at an earlier one. Id order was rejected: ids
  follow first use and say nothing about dependence.
- **h. Where it lives.** `World` owns one `Resources` and exposes `GetResources()`.
  `SystemContext::Get<T>()` forwards to it, and systems use `Get<T>()` by convention. Only the id
  is static. The values belong to the World, so a second World holds its own.
- **i. Tests.** Five, in the ECS-only targets, so no SDL: insert a value and read it from a system;
  two Worlds hold different values; `Has` and `TryGet` before and after `Insert`; the destructor
  runs when the World dies, in reverse insertion order; a value with a non-trivial type works,
  such as a `std::vector`.

**What the build checked.**

- **Tests.** `scripts\run-tests.bat` passes in all four configurations. `EcsTests` runs 376 checks,
  25 of them in a new "resources" section:
  - A `Score` goes in. `Has`, `TryGet` and `Get` find it at the same address, a write through `Get`
    changes the stored value, and a type never inserted stays absent.
  - A system reads the `Score` through `context.Get<Score>()` in `RunPhase`.
  - A second World holds its own `Score` at another address, and the first World's value does not change.
  - `PlayerNames`, which owns a vector of strings, takes its constructor argument from `Insert`, keeps
    its address after later inserts, and keeps its strings.
  - A `Resources` built on a counting memory resource takes three blocks and gives three back. Its
    destructor runs three loggers in the order 3, 2, 1. A World also destroys its loggers.
  - `Set` over an existing value keeps the address and replaces the value. `Set` on a missing type
    inserts it, and a second `Set` keeps that address too.
  - Not covered: the asserts on a second `Insert` and on `Get` of a missing type. A failed assert
    stops the program.
- **A bug the tests found.** The first build crashed `EcsTests` with a segfault in x64 Debug. The
  `~Resources` loop used `count - 1` on a `uint32_t`, so a store with no resources wrapped to
  4,294,967,295 and called a null destroy function. It now counts down from `count`.
- **The game,** in Debug, through `scripts\run-game.ps1`: two runs, `-Scene 1` with F3, F4 and F1, and
  `-Scene 0` with F2, F3, F4 and F1. Both ended with exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0 asserts,
  0 fell to the heap and 0 leaked blocks. The keys switched through 1 scene in the first run and 3 in
  the second, so no run covered all four scenes by key. Step 2 does not touch the scenes yet.

**Open questions.** `System` declares the components it reads and writes; Bevy and Mass declare
resource access the same way. Single threaded, that only documents and checks, so it can wait. The
first users after this plan are the networking scene's `NetworkRole`, sockets, `ClientTable`,
`SnapshotBuffer` and `NetworkIdMap`.

## 3. Move the context fields

**The problem.** Some of today's `SystemContext` fields change by phase: `deltaTime` is 1/60 s in
Fixed and the frame time in Update and Extract, and `drawList` exists only in Extract. The engine
writes these values and systems only read them. Bevy and Unity Entities do the same: Bevy overwrites
its `Time` resource when `FixedUpdate` runs, and Unity keeps a time value per World.

**Decisions.** Agreed 2026-11-18. Decision b changed on 2026-11-19, when `Set` arrived in step 2.

- **a. Time.** A one-float struct, `DeltaTime`, lives in `ecs`. The registry sets it with `Set` at
  the start of each `RunPhase`, because it already holds the number and knows which phase runs. Putting it in `core` was
  rejected: the registry could not name it, so `Application` would write it before every phase. It
  can move there later with no other change.
- **b. Input and camera.** `Application` calls `Set` with `InputState` and the scene's `Camera` on
  the World's resources once a frame. The camera copy comes after `Scene::Update`, because the fly
  camera moves there. Both are plain data that change once a frame, so a copy has no null and no
  dangling pointer. `Set` inserts on its first call, so nothing needs an insert step. The first
  version of this decision had an `InsertEngineResources(World&)` function that ran after each scene's
  `OnCreate`, but `SceneManager` lives in `core` and cannot call code in `app`, so it was dropped.
  `Scene::GetWorld()` becomes non-const so `Application` can refresh the resources. Holding pointers
  to the owners was rejected.
- **c. Draw list and debug lines.** Each gets a small holder struct in `core` with one pointer,
  `DrawListTarget` and `DebugLinesTarget`. Before Extract, `Application` sets each to point at the
  frame's list. After Extract, it sets each back to null, so a system that asks for a list outside
  Extract never reads freed arena memory. The null rule stays as today, but `SystemContext.h`
  stops naming the types. The lists live in the frame arena, which resets every frame, so a vector
  stored in the World would point at freed memory. A second render World like Bevy's was rejected:
  a big change with no caller. The call changes if the lists move off the frame arena.
- **d. No `Remove`.** Time, input and camera are always present. The holders stay in the store and
  only their pointer changes. Step 2's API holds.
- **e. Reading.** `SystemContext` keeps `world` and `commands` and gains `Get<T>()`, which forwards
  to the World's `Resources`. It needs `World.h` and the standard library only. Every system that
  read `context.deltaTime`, `drawList`, `debugLines`, `inputState` or `camera` reads
  `context.Get<T>()` instead. `RunPhase` shrinks to `RunPhase(phase, deltaTime)`.

**What the build checked.**

- **Tests.** `scripts\run-tests.bat` passes in all four configurations. `EcsTests` runs 376 checks.
  Four are new in the "systems" section and one is gone:
  - A World holds no `DeltaTime` before its registry runs a phase.
  - The registry leaves the phase's `DeltaTime` in the World's store.
  - A later phase overwrites it, and its systems see the new value.
  - The old "a system outside Extract gets no draw list" check is gone, because the registry no longer
    sets lists. `Application` does.
- **The game,** in Debug, through `scripts\run-game.ps1 -Scene 0`: F2, F3 with a mouse drag, F4, F1.
  It went through all four scenes and ended with exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0 asserts, 0
  fell to the heap and 0 leaked blocks. In `PhysicsScene` the dragged cube turned, so picking works
  through the input and camera resources. The screenshots of the other scenes look as before.
- **Speed,** in Release x64, through `scripts\run-game.ps1 -Config Release -Scene 0` with F3 and F4
  after 6 s and 11 s, 16 s a run. The old code (`2cafe20`) and the new code alternated: one cold run
  each, dropped, then three runs each. The numbers are the profiler's average per span, in ms,
  mean of 3:

  | span | old | new |
  |---|---|---|
  | whole frame | 4.2057 | 4.2070 |
  | Render | 4.1723 | 4.1733 |
  | MakeDrawList | 0.0240 | 0.0243 |
  | DrawRenderables | 0.0190 | 0.0187 |
  | DrawColliders | 0.0090 | 0.0097 |
  | Record | 0.0700 | 0.0697 |
  | WrapIntoBox | 0.0030 | 0.0030 |
  | UpdatePosition | 0.0020 | 0.0010 |
  | Scene::Update | 0.0020 | 0.0020 |

  The frame time is pinned at about 4.2 ms by the 240 Hz display (FIFO), so it hides a change of
  microseconds. The profiler prints 0.001 ms steps, and every difference above is one step or less.
  A cost under that cannot show here. `EcsBench` does not run the registry, so it could not help.
