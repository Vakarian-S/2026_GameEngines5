# Roadmap

**Last updated:** 2026-11-05.

Work that is planned but not active, grouped by area. PLAN.md holds only the phase in progress. When a
block here becomes the next phase, it moves to PLAN.md and gets designed piece by piece with Vakarian.

Every block uses one table, `# | builds | why | status`. Notes under a table are free form. A row's
status is **to-do**, or **in progress** once its block has moved to PLAN.md. A finished row leaves
this file.

Started 2026-10-23 with the findings from the prototype games. findings/2026-10-prototype-game.md
(the cube arena and the shooter) and findings/2026-10-space-prototype.md (the space flight course)
hold the measurements behind most rows.

## Contents

- [ECS](#ecs)
  - [Hardening](#hardening)
- [Physics](#physics)
  - [Collision](#collision)
  - [Fixed step additions](#fixed-step-additions)
- [Math](#math)
  - [Owning the math library](#owning-the-math-library)
- [Rendering](#rendering)
  - [Frustum culling and transforms](#frustum-culling-and-transforms)
  - [Frame pacing and the present mode](#frame-pacing-and-the-present-mode)
  - [Basic shapes](#basic-shapes)
  - [Model import settings](#model-import-settings)
- [Debug tools](#debug-tools)
  - [Persistent debug lines](#persistent-debug-lines)
  - [A shipping build](#a-shipping-build)
  - [Automated game tests](#automated-game-tests)
- [Course topics](#course-topics)
  - [Semester topics](#semester-topics)
  - [The Ubisoft Next contest](#the-ubisoft-next-contest)
- [Miscellaneous](#miscellaneous)
- [Code cleanup](#code-cleanup)

## ECS

### Hardening

Step 3 measured `Playback` on 2026-10-26: about 0.6 microseconds per frame in Scene0, down from about
0.7, so it did not grow and this block did not need to come first. None of it blocks the course work:
the physics scenes hold a few hundred entities, created once.

| # | builds | why | status |
|---|---|---|---|
| 3 | Handles inside component data resolved at playback, or a create that returns the real handle | The command buffer resolves placeholders only in a command's target, never inside a component's bytes. So today an entity that holds another entity's handle must be built straight into the World in `OnCreate`. Create with components is built (dev-notes/create-with-components.md), so a create already carries every value in one command | to-do |
| 5 | A store on the World for world-unique data, keyed by type, that `SystemContext` reads from | Wanted since step 3, so that `ecs` names no render type. `deltaTime`, the draw list, the debug lines and the camera are each one per world, and each one is a hardcoded field on a struct every system includes. Bevy calls these resources, Unity uses singleton entities. Keyed by type, not one shared struct | to-do |
| 6 | Run conditions: `Add` takes an optional function that reads world data and answers run or skip. `RunPhase` asks it before each run. The C and M toggles become a `DebugView { showWires, showMeshes }` in the store from 5, written by an ordinary system in Update | Step 4a switches systems with a flag on `System` that only `SystemRegistry::SetEnabled` flips. That works for two toggles, but the on/off state hides in the registry. As data in the world, it can be printed, saved and shown like any component, and one condition (a `GamePaused`, say) can gate many systems at once. Bevy's `run_if` and Unity's `RequireForUpdate` work this way. The flag's `continue` in `RunPhase` stays; only the question changes. Needs 5 | to-do |
| 8 | Stable type identity: identify a type by a hash of its compile-time name, keep the dense id for runtime, and register in the exe only | `IdOf<T>` is a header static, so each DLL gets its own counter, and the same mask means different things across the boundary. Needed by the first DLL, save or network packet | to-do |
| 9 | Faster migrations: a column lookup table per archetype (`FindColumn` becomes an array read), walking only the shared bits of the two masks in `MoveEntity`, and an archetype edge cache (the archetype reached by adding component X) | Only when the counters show migration cost. Measured 2026-09-30 with `EcsBench`: 0.114 to 0.124 microseconds per migration from 1,000 to 100,000 entities, and 24,000 migrations fit in a 4.2 ms frame. Missing 60 fps would take about 130,000 migrations in one frame (dev-notes/profiling.md) | to-do |
| 10 | Enableable components: one bit per entity per component, toggled with no structural change, honoured by queries | The answer if archetype churn ever dominates. Adding a tag is a full row copy here, against a push into one pool in EnTT. The counters say when | to-do |
| 11 | Writes to other entities sorted by target first | A damage system that reads a contact and writes `Health` on another entity chases a pointer per write. Sorting the events by target is the usual answer. Only once it shows in a profile | to-do |
| 13 | A chunk size per archetype, overriding the World's `chunkBytes` | Only once the 4, 16 and 64 KB benchmark (dev-notes/ecs-chunks.md) shows that different archetypes want different sizes: a wide row fits few entities in a small chunk, and a tiny row wastes a large one. Unreal Mass has this as `ChunkMemorySize` in `FMassArchetypeCreationParams`, where 0 means the default | to-do |

**Row 9, a further option: power-of-two rows per chunk.** Round `rowsPerChunk` down with
`std::bit_floor`, and the row-to-chunk lookup becomes `row >> rowShift` and `row & rowMask` instead of a
divide. The cost is chunk space: a row that fits 300 times gets 256, and one that fits 511 times wastes
about half the chunk. The lookup runs only in `GetComponent` and migrations, never in the chunk loops,
so it comes with row 9's trigger: the counters show migration cost.

**Row 10, the alternative: sparse-set storage.** Bevy lets a component opt out of the archetype tables
and live in a sparse set. Adding and removing it then moves nothing, but iterating it gets slower. Bevy
suggests it for per-frame markers and short status effects; here that means `Picked`, `Falling` and
`DebugWatch`. Same trigger as row 10: the counters show archetype churn. Weigh both when it fires.

Order: 6 needs the store from 5. Rows 1, 2, 4, 7 and 12 were built by the ECS hardening plan that
ended on 2026-11-05 (dev-notes/README.md, "Plan steps"); the numbers of the rows left keep their
old values, so references to them still hold.

## Physics

### Collision

Shapes in `PhysicsScene` pass through each other and through the floor, and nothing stops a released
shape from falling forever: F3 restarts the scene instead. Step 4 left collision out of scope.

| # | builds | why | status |
|---|---|---|---|
| 1 | Shape-against-shape collision: detect contacts between the four colliders, then resolve them | The colliders and their ray tests exist since 4b, and the constraint solver since 4c, so a contact can become one more constraint in Fixed | to-do |
| 2 | A floor | The cheapest first contact, and it ends the endless fall | to-do |

### Fixed step additions

From step 3's design of `FixedStep`. Each comes when something needs it.

| # | builds | why | status |
|---|---|---|---|
| 1 | `Alpha()`: the leftover time divided by the step, for interpolation | A frame can run 0 or 2 fixed steps, which can stutter. Interpolating draws between the last two steps hides it | to-do |
| 2 | Pause and single step: freeze physics and advance one step per key press | For debugging physics frame by frame | to-do |
| 3 | A step counter | For networking, replays, or comparing two runs | to-do |
| 4 | A time scale, for slow motion | If it must also slow Update, pause and time scale may need a third class between `Timer` and `FixedStep`, as Bevy's `Time<Virtual>` | to-do |
| 5 | The step size as a setting | If constraints need 120 Hz | to-do |

Glenn Fiedler's "Fix Your Timestep!" is the standard write-up.

## Math

### Owning the math library

Decided 2026-10-28: the course math library is ours now. It has been edited many times (`kPi`, the
trivially copyable types, `kVerySmall`), and 4b deleted parts of it.

| # | builds | why | status |
|---|---|---|---|
| 1 | Move it out of `external/` into the engine's own folders | The folder still says "someone else's code". A namespace should match its folder | to-do |
| 2 | Restructure it, and drop what nothing uses | The `MATHEX` dual quaternion and 2D sets may be unused. Check before deleting | to-do |
| 3 | Move general math into `MATH`: `PHYSICS::SolveQuadratic`, and maybe `PHYSICS::Ray` | They are not physics. Until the move they stay in `PHYSICS`, so each name matches its folder | to-do |
| 4 | Add license files for `external/MathLibrary` and `external/glm` | Neither came with one. Needed before the repository goes public | to-do |
| 5 | Turn `VERY_SMALL` and `VERY_CLOSE_TO_ONE` into constants | They are still macros in `ConstantsConversions.h`, where `kPi` already became a constant | to-do |

## Rendering

### Frustum culling and transforms

These three changes need culling, or the ECS hardening above, before they pay off. The camera already
reaches systems: `SystemContext` has carried a `const Camera*` since step 4b.

| # | builds | why | status |
|---|---|---|---|
| 1 | `StaticMatrix`, a matrix component built once at load. Holding it means the entity never moves, so the TRS build skips it with the query's `none` mask and the draw system reads it straight | Culling is its second reader. To weigh: a 64-byte matrix in the same archetype as gameplay data halves the rows per block, which raises the copy cost of every structural change. Unity stores `LocalToWorld` and Bevy `GlobalTransform`, both derived, both written by one system, both documented as stale for part of the frame. Cache only what never moves: a mover pays the build either way, and 4,001 of them cost 0.124 ms | to-do |
| 2 | Parent and child transforms | Scene3's turrets copy their parent's transform by hand. Parents must update before children, and archetypes group by which components an entity has, not by a value such as depth (house rule 10). The candidates: sort blocks by depth, make depth part of archetype identity as DOTS does, or rebuild a per-depth index. Needs 1 | to-do |
| 3 | Shared fragments, and one instanced draw per archetype | Scene3 drew 2,944 items one draw at a time. A shared value splits archetypes, so it needs ECS hardening step 1 | to-do |

Rejected 2026-10-26 for row 1, after reading how Unity, Bevy and Legion do this. **A per-frame dirty
tag:** adding and removing a component is a structural change at about 8 migrations per entity, so
marking 4,000 movers would cost thousands of migrations a frame to save 0.124 ms. No shipping ECS
marks dirtiness by adding a component. **A write version per column:** nearly free to track, but an
archetype holds every row in one run, so one drifting rock dirties all 4,000 beside it. Worth
revisiting once ECS hardening step 1 splits the columns into blocks.

A camera **entity** is not planned. The trigger for it is the engine having to loop over cameras:
split screen, a shadow or minimap view, a camera rendering to a texture. One view is a field; several
views are a column. Unity keeps the camera out of the ECS for this reason, and Bevy puts `Camera`,
`Frustum` and `VisibleEntities` on one entity so that N views are N entities. Follow cameras, shake
and splines do not need it, and `FlyCameraController` already shows that.

### Frame pacing and the present mode

From the space flight prototype, 2026-10-26. The loop ends every frame with
`SDL_Delay(timer.GetSleepTime(60))`, so the profiler's frame total reads about 16.3 ms whether the
scene holds 600 entities or 4,000. The per-span numbers are sound; only the frame total is blind.

| # | builds | why | status |
|---|---|---|---|
| 1 | The present mode as a setting, defaulting to FIFO when vsync is on | FIFO is the only present mode Vulkan guarantees, and the driver then paces the loop inside `vkAcquireNextImageKHR` with no sleep and a genuinely idle CPU. The swapchain asks for MAILBOX first today, and MAILBOX never blocks: with the sleep gone the prototype reached 6,076 fps. The AMD 610M offers no MAILBOX, so the same binary falls back to FIFO there and behaves differently | to-do |
| 2 | The frame cap as a value rather than the literal 60, owned beside the window | The displays on this machine run at 239 Hz and 165 Hz, and the engine draws 62.5 frames a second on them | to-do |
| 3 | A precise sleep for a cap below the refresh rate: `SDL_DelayNS` to about a millisecond short, then a short spin | Vulkan has no present mode for 60 fps on a 239 Hz display, so this case needs a sleep. SDL stays the right layer: sleeping is an OS service, and `SDL_GetTicksNS` gives a monotonic clock | to-do |

### Basic shapes

From physics step 4a's design, 2026-10-27. Generated meshes for the basic shapes, beside the existing
`CreateCube()`, each taking its size the way the matching collider does. Wanted for level blockouts,
placeholders, prototypes and test scenes; all three prototype games were built from cubes. Not needed
for step 4: the physics scene reuses Mario and the cube.

| # | builds | why | status |
|---|---|---|---|
| 1 | `CreateSphere()`, `CreateBox(halfExtents)`, `CreateCylinder(radius, halfHeight)`, `CreateCapsule(radius, halfHeight)` on `ResourceLoader`, generated in code and cached per size | `Transform.scale` is uniform, so it cannot change a shape's proportions: a 1 m cube mesh can never become a 2 x 1 x 4 box. A mesh generated at the collider's sizes matches it at any proportions, and scale still multiplies both. The sizes follow the collider convention, half sizes measured from the centre. A ball has one size, so one unit sphere and `scale` cover every ball. Godot builds its primitives this way (`CapsuleMesh` has `radius`, `height`, `radial_segments`, `rings`); Unity's `CreatePrimitive` uses fixed built-in meshes | to-do |

**Decided against: a spawn helper** that creates the mesh, the entity and the matching collider in one
call, as Unity's `CreatePrimitive` does. Creating a mesh and creating a collider are separate actions:
the mesh is a shared asset in `ResourceStore`, and the collider is a component on one entity. Nothing
links them except the same numbers passed to both. Keeping the two calls visible in scene code keeps
that clear.

### Model import settings

From physics step 4a's design, 2026-10-27. A collider is centred on the entity's origin and sized by
`Transform.scale`, so a model in the wrong units or with its origin off centre puts the collider in
the wrong place, or makes every collider number read in odd units. Not needed while the physics scenes
use centred unit meshes.

| # | builds | why | status |
|---|---|---|---|
| 1 | Import settings passed with the load, such as `CreateMesh("Mario.obj", {.fitSize = 1.0f, .recentre = true})`, and part of `ResourceStore`'s cache key | The store caches meshes by path and shares them between scenes, so a separate `AdjustMesh` call after the load would change the mesh for every scene and miss the copy already on the GPU. With the settings in the key, the same file with and without them becomes two meshes | to-do |
| 2 | An import scale: a known unit factor per file (centimetres to metres is 0.01), or fit to a size from the bounding box | Keeps `Transform.scale = 1` meaning "the size the model is meant to be", so collider numbers are plain metres and scale stays free for gameplay. A uniform scale cannot deform the mesh: positions are multiplied by one number, and normals, UVs and indices do not change | to-do |
| 3 | Recentre: subtract the bounding box centre from every vertex | The engine can measure where the origin sits but cannot know whether it was meant: a character's origin at the feet lets it stand at y = 0, and a door's sits on its hinge. So recentring is a choice made per model, never automatic. Where the origin is meant, the alternative is an offset on the collider, which Unity's colliders have as `center` and step 4a left out | to-do |
| 4 | The facing fix: turn a model that faces +Z to the engine's -Z at load | Space prototype finding 8. `Mario.obj` faces +Z, and the prototype paid a quaternion multiply per draw for it | to-do |

## Debug tools

### Persistent debug lines

| # | builds | why | status |
|---|---|---|---|
| 1 | A line added with a lifetime that `core/debug` keeps between frames | A system outside Extract could then draw once. Step 4b draws its pick ray each frame from a `PickRay` component instead. Unreal's `DrawDebugLine` takes a persistent flag and a lifetime (from memory, not checked) | to-do |

### A shipping build

From physics step 4a's design, 2026-10-27. Debug drawing (the `core/debug` line helpers and debug
systems such as `DrawColliders`) runs in every build today, turned on and off at runtime with keys, the
way PhysX ships its visualization switched off.

| # | builds | why | status |
|---|---|---|---|
| 1 | A define of its own for debug drawing, such as `GE5_DEBUG_DRAW`, wrapping the helpers and the lines that register debug systems, once the engine has a shipping configuration | Not `_DEBUG`: the wires are wanted in optimized builds too, for example while profiling a physics bug in Release. Unreal does the same, wrapping debug drawing in `UE_ENABLE_DEBUG_DRAWING` and removing it only from Shipping builds. An environment variable cannot do this job: it picks settings at runtime but cannot take code out of the binary | to-do |

### Automated game tests

From 2026-11-05. `scripts/run-game.ps1` drives the game from outside: it sends keys and mouse input
through Windows and photographs the window. Its first runs found a real crash (Scene1's world arena
fills at startup). It needs the desktop and the window's focus, its timing is wall-clock time, and its
screenshots depend on the screen. These rows move the same job inside the engine, so a run needs no
desktop and plays the same frames every time.

| # | builds | why | status |
|---|---|---|---|
| 1 | Input replay: a file of "frame N: this input" that fills `InputState` instead of SDL | Input enters in one place (STATE.md, architecture rule 8), so a replay changes nothing else. With the fixed step, a replay plays the same frames on every run, with no focus or desktop | to-do |
| 2 | Frame capture: the renderer saves a frame to a PNG on request | An exact image of what the GPU drew, with no screen, window border or DPI in it | to-do |
| 3 | Image compare: save a known-good image once, compare each run against it | Catches visual changes no log shows. Needs 1 and 2, so the same frames give the same image | to-do |
| 4 | `run-game.ps1` starts the exe with a replay file, and the PowerShell input goes | Once 1 and 2 exist, the script only starts, waits and reads the summary | to-do |

## Course topics

### Semester topics

The course topics not started yet. Effort is the guess from 2026-09-27, for one person new to each
topic. When a topic is done, it leaves this table and STATE.md's table says so.

| # | builds | why | status |
|---|---|---|---|
| 1 | Controller support | Effort small. Does not change the ECS | to-do |
| 2 | Audio | Effort small to medium. Does not change the ECS. The audio system is the one place allowed to keep "what changed since last frame" state, as house rule 7's exception for a retained API says | to-do |
| 3 | File I/O | Effort small to medium. Does not change the ECS | to-do |
| 4 | DLLs | Effort small. Does not change the ECS | to-do |
| 5 | Frustum culling | Effort medium. Does not change the ECS: it is a system. See Rendering | to-do |
| 6 | Concurrency | Effort medium. Barely changes the ECS: one command buffer per thread, merged at the sync point | to-do |
| 7 | Networking | Effort large. Changes the ECS, additively: a `NetId` component and a map from remote to local entities. As in EnTT's `continuous_loader`, the map must also rewrite entity handles stored inside components | to-do |

- Rows 1 to 4 are the small batch, done one at a time between larger steps. Rows 5 and 6 build on the
  systems and phases from step 3. Networking goes last, because it is large and risky.
- Never do two long tasks back to back. After a long one, do a small topic and finish it.
- **Controller support, and a later player controller.** Step 4a's design named the next layer: a
  `PlayerCommand` for game commands (move, fire), built once per fixed step and reset at the start of
  each step, as Overwatch builds one per simulation tick. Per-frame readers keep reading
  `InputState`, so a visual effect follows the key at frame rate while the simulation follows its own
  step. Until it exists, a press needed in Fixed uses a latch: Update sets a flag, and the next fixed
  step uses and clears it. dev-notes/physics-scene.md has the input design it builds on.
- **Concurrency.** Step 3's design lists what threads need beyond the command buffers: a
  reader-and-writer check, `Conflicts(a, b)`, next to `Print`'s shared-writes note; one command
  buffer per thread, merged in a fixed order; a thread pool; and declared reads and writes for shared
  data that is not a component, such as the `DrawList`. A render thread would also need one frame
  arena per frame in flight. A guess, not measured: if the three habits in ECS.md's "Systems and
  phases" hold, this is one class of a few hundred lines plus one line per system, and no system
  body changes.

### The Ubisoft Next contest

| # | builds | why | status |
|---|---|---|---|
| 1 | A second render backend for the contest's API, not a replacement | The seam exists: a scene fills a `DrawList` and never sees a Vulkan type (dev-notes/vulkan-renderer.md, "Two render backends") | to-do |

**Open question: reuse this engine, or rebuild it?** The contest encourages building the solution
yourself. Rebuilding the ECS from memory would prove the archetype storage, the command buffer and the
placeholder scheme are understood. Against: it is time not spent on the course list, and the engine
exists. It need not be all or nothing: rebuild the ECS, and reuse the tooling (MemoryMonitor, the
tests, the build setup).

## Miscellaneous

Small changes that can land between any two steps.

| # | builds | why | status |
|---|---|---|---|
| 1 | A colour in `MaterialDesc` | A material's colour comes only from a texture today. Scene3 needed ten PNG files only for colours | to-do |
| 2 | A light budget in the renderer | The renderer throws past `kMaxLights` today | to-do |
| 3 | A way to pass settings to a scene | Scene3 read autoplay from an environment variable | to-do |
| 4 | Fix `Timer::GetSleepTime`'s truncation | `1000 / fps` truncates, so a cap of 60 targets 62.5 fps and any cap above 1000 sleeps not at all | to-do |
| 5 | Skip `SDL_Delay` when the sleep is zero | `SDL_Delay(0)` still yields to the scheduler: skipping it took a frame from 0.644 ms to 0.165 ms. `GetSleepTime` returns zero whenever a frame overran its budget, so a game that drops below its target pays extra scheduler time exactly when it is already struggling | to-do |

## Code cleanup

Known leftovers in the code, moved from STATE.md on 2026-11-03.

| # | builds | why | status |
|---|---|---|---|
| 1 | Automated tests for `PickSystem`, `DrawPickRaySystem` and the dangle systems | Nothing runs them automatically. Vakarian checked them by hand on 2026-10-30, 2026-11-02 and 2026-11-03; `PhysicsTests` covers only the math they call | to-do |
| 2 | A check that a system's query asks for the components it reads: an assert in `Column`, or a check in the registry | The read mask is only a label. In 4c, a query copied from another system matched archetypes without `Picked`, so `Column<Picked>()` returned null and crashed. It happened again with `AngularVelocity` missing from `ApplyForcesSystem`'s query | to-do |
| 3 | Measure what `PhysicsScene`'s world arena needs | `kWorldArenaBytes` went from 16 KB to 256 KB in 4c without a measurement | to-do |
| 4 | Remove `ResourceStore::CreateMesh`'s `std::cout` mesh counts | Leftover debug print | to-do |
| 5 | One `EcsTests` check through a `const World&` | The const `World::GetComponent` has no caller, so no build compiles its body. A scratch compile on 2026-10-27 showed it works | to-do |
| 6 | A named local per cube orientation in `Scene1` | It builds both in one long `?:` inside the initializer | to-do |
| 7 | A forward declaration instead of `Buffer.h` in `MaterialDescriptors.h` | The include is not needed | to-do |
| 8 | Remove the four spaces before `#version` in `phong.vert` | Tidiness | to-do |
| 9 | Narrow MemoryMonitor's logging to entity creation | Logging covers a scene's first `OnCreate`, asset loads included, so the OBJ parse floods the log with about 19,900 lines | to-do |
| 10 | Rename `fragWorldColor` to `fragColor` in `debug.vert` and `debug.frag`, and comment their inputs and outputs | The colour is not in world space, and `phong.vert` comments each one | to-do |
| 11 | A `reserve` in `LoadObjFile` | It grows its vectors and map one item at a time. Not measured; the load runs once per program because of the cache | to-do |
