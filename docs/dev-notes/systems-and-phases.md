# Systems and phases: a registry runs every loop

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed 2026-10-23 and 2026-10-24, built 2026-10-24 to 2026-10-26 in seven builds, as step 3 of the
plan that ended on 2026-11-03. The rules that still hold are in living/ECS.md, "Systems and phases".

## Sources

- Unity's `FixedUpdate`, `Update` and `LateUpdate`; Bevy's `FixedUpdate`, `Update` and `Extract`;
  Unreal's tick groups around physics. All three mix phases the same way.
- Unreal Mass: each processor is a class that owns its queries.
- Bevy's split of a system's code from its schedule, and its `Time<Real>`, `Time<Fixed>` and
  `Time<Virtual>`. Unity keeps all time on one `Time` class; Godot has a separate `MainTimerSync`.
- Glenn Fiedler's "Fix Your Timestep!", the standard write-up of the accumulator.
- Unreal's `GetSafeNormal` and `GetUnsafeNormal`.
- Meyers, *Effective C++*, item 3, for the rejected `const_cast` version of `Column`.
- The Scene2 and Scene3 prototypes: findings/2026-10-prototype-game.md. Finding 5 (the copied draw
  code) and finding 13 (no const `Column`) fed this step.

## What it does

Every loop that touches the world runs as a system. A scene creates its systems once and adds each
one to its `SystemRegistry` under a phase. Each frame, `Application` runs the phases in order: Fixed
0 or more times at 1/60 s, then Update, then Extract. After each phase, and after each fixed step,
the registry plays back the command buffer. Startup prints each phase with its systems, their reads
and their writes. The profiler names each system.

Step 3's systems were Movement, Damage, Spin and DrawRenderables, in Scene0 and Scene1. Step 4c later
split Movement and deleted Spin: see dangle-physics.md.

## Phases

| phase | runs | holds | step 3's systems |
|---|---|---|---|
| Fixed | 0 or more times per frame, 1/60 s each | anything that moves things over time | Movement, Spin |
| Update | once per frame, with the frame's time | game logic | Damage |
| Extract | once per frame | reads the world and fills the `DrawList` and `DebugLines`. Writes no component | DrawRenderables |

- The enum order is the run order. Every system in one phase runs before every system in the next.
- A phase exists only for a different clock (Fixed against Update) or a sync point (a system needs
  another system's queued changes in the world first). Not for a lifecycle or a feature.
- One rate for the whole Fixed phase. Physics needs every fixed system on one clock. A system that
  runs every N steps keeps its own counter.
- Inside a phase, systems run in the order the scene adds them. Nothing sorts them.
- Two writers of one component may share a phase. On one thread, the add order decides who goes
  first. Gravity and a push can both add to `Velocity` before Movement reads it. This removed the
  need for a separate `FixedSolve` phase.
- The command buffer handles only create, destroy, add and remove. A value write goes straight into
  the column, so add order, not the buffer, decides which of two value writes wins.

**Three habits keep threads possible.** They cost nothing on one thread.

1. A system reaches the world, the buffer and the lists only through `SystemContext`, never through
   the scene. Later the registry can give each thread its own context.
2. A system keeps no shared state across rows, such as a static counter. Any split of the rows then
   gives the same result.
3. The order stays manual. A later scheduler may run neighbours side by side when they do not
   conflict, but never reorders them.

## What a system is

- A small class that derives from `System`. It owns its query and its state, and has one function,
  `Run`. The registry owns every system and calls `Run`.
- State is the private members kept between calls: the query and its cache of matching archetypes,
  counters such as Damage's run count, settings such as Movement's box half width. Each changes at
  most once per `Run`, never per row, which keeps habit 2.
- One instance per scene. The scene creates it in `OnCreate`, and it dies with the scene. A system
  built every frame would rebuild its query cache and reset its counters each frame.
- The constructor takes the world only to build the query, because `Query` keeps a `World*` to
  rebuild its cache when the world makes a new archetype. The system stores no world pointer.
  `Run` uses the context's world for everything else.
- `SystemContext` is one per phase, the same for every system in it: the world, the command buffer,
  the time step, and the draw list and debug lines, which are null outside Extract.
- The name, reads and writes describe the system's code, so they live on the `System` base class.
  The constructor fills them and nothing changes them later. The phase is a scheduling choice, so
  the scene gives it when it adds the system. Bevy splits it the same way.
- Names are for people only: the startup print, the profiler table and the shared-writes note. Code
  never looks a system up by name. The name must be a string literal, because the profiler keeps it
  by pointer. Two instances of one class share a name, and the profiler adds their times.
- A structural change counts as a write: Damage adds and removes `Hit` through the buffer.

**The const `Column`.** `Archetype::Column<T>() const` returns `const T*`, with a const copy of the
private `FindColumn` behind it. Extract systems loop over `const Archetype*`, so the compiler stops
a write. `std::vector::data()` has the same pair. Each body is two lines, so writing it twice costs
little, and the compiler checks both copies. It does not check a system's reads against its code: a
system can still take the non-const column.

**Where the files went.** `System`, `SystemContext` and `SystemRegistry` sit in `include/ecs` and
`src/ecs`, where `EcsTests` can test the registry with fake systems. `SystemContext.h`
forward-declares `DrawItem` and `LineVertex` and holds pointers to their vectors. A pointer to an
incomplete type compiles without its definition, so `ecs` still includes no core header, math
library or SDL. The cost: `ecs` knows two render type names. The game systems sit in
`include/game/systems` and `src/game/systems`, one header and one `.cpp` each.

## The registry

- It owns the systems through `unique_ptr`, because each is a different class behind `System`.
- It owns the command buffer, moved from Scene0 into the registry. Playback after each phase is the
  registry's rule, so no caller can forget a playback. The scenes lost their `commandBuffer` member.
- It holds the world by reference. The scene declares the world first, so the world outlives it.
- One list per phase, indexed by the phase. `Phase` has a last entry, `Count`, for the array size.
- Three functions: `Add` pushes a system onto a phase's list, `RunPhase` builds the context, runs
  the phase's systems in order and plays back, and `Print` writes the startup report.
- `Print` compares each pair of systems in one phase and prints a note when their writes share a
  bit. A note is not an error: it says that moving one system changes the result. Read against write
  gets no note, because add order is the intended order: Gravity before Movement is the point.
- `SceneManager::Start` calls `Print` right after `OnCreate` succeeds, so every visit prints and no
  scene can forget it.

**What the profiler sees.** `RunPhase` opens a plain profiler scope named after each system around
its `Run`. A system never changes the world's structure directly, so a plain scope is enough. When
the buffer holds commands, an ECS scope wraps the playback, named "Playback Fixed", "Playback Update"
or "Playback Extract". The ECS scope counts migrations. An empty buffer gets no span, so the table
shows how often each phase really plays back.

## The fixed step

- One accumulator, a step of 1/60 s, a cap at 0.25 s. Frames of 16, 17 and 50 ms run 0, 1 and 3
  steps, and the leftover carries over. Without the cap, a long stall such as a breakpoint or a load
  would queue hundreds of steps, each making the frame slower.
- `FixedStep` is a small class in `include/core/FixedStep.h`, not a float in `Run`. It holds no SDL,
  so a test feeds it frame times and checks the step counts. Its rules (`>=` against `>`, the cap
  before or after adding) fail quietly, as cubes moving a little too fast or slow.
- `Application` owns it, next to the `Timer`. There is one fixed rate for the program.
- A scene switch resets it and restarts the `Timer`. A first visit's `OnCreate` takes about 128 ms in
  Debug, so without the restart the new scene's first frame would run 7 catch-up steps.
- `Timer` reads the OS clock through SDL; `FixedStep` is plain arithmetic. They stay two classes.

## Scenes and wiring

- `Scene` gained `GetSystems`, and lost `MakeDrawList` and `MakeDebugLines`: their loops became
  Extract systems. `Update` stayed for work outside the world: the fly camera and
  `world.ResetFrameStats()`.
- `Application::Run` kept the two outer spans, "Scene::Update" and "MakeDrawList", with their old
  names, so Scene0's before and after compare the same work.
- The camera, the lights and Scene1's lighting scheme are not in the world, so the scene keeps them.
  The camera can become an entity once there is a reason, such as a follow camera.
- `Application` owns the `DrawList` and `DebugLines` for one frame, in the frame arena. A scene's job
  shrinks to two things: give entities the right components, and register the right systems.
- One `DrawRenderablesSystem` replaced the draw loop the prototype copied three times.
- The grid and axes stay in `Application`, added right after Extract, with the G toggle unchanged.
  They read no world, like the profiler.
- **Scene0 dropped its scaffolding**, so every system is reusable. The first 100 cubes get a
  `Damageable` tag, and `DamageSystem` scans for it and toggles `Hit` every 60 runs. The scene lost
  `entities`, `frameNumber` and `hitOn`. Each cube gets a `Renderable` with material `i % 2`, so
  neighbours still alternate, and a `Rotation` of 0, because queries have no optional components.
  `MovementSystem` takes the box half width in its constructor. The cost: two more columns per cube,
  and a rotation matrix the draw loop does not need.

## Safety

- **The math library's throws.** In Debug, `external/MathLibrary` throws a `std::string` at 28
  sites, such as `normalize` on a zero vector. `main` caught only `std::exception`, so the program
  exited with code 3 and no message. `main` gained two catches: `const std::string&` prints
  `[ERROR] math library:` and the message, and `...` prints `[ERROR] unknown exception`. Both
  return 1. They only report, so the bug gets fixed, not hidden.
- The library stays untouched. Normalizing a zero vector is usually a bug, and the Debug throw
  catches it where it happens. `SafeNormalize(v, fallback)` in our own code covers the places where
  zero is a normal state, such as no key held. The root causes so far were all ours: the fly camera
  and Scene2's player with no key held, and Scene3's debris shrinking toward size 0.
- **The frame arena.** Scene3 put 2,944 draw items (212 KB) into the 128 KB arena, and 1 frame in 6
  fell to the heap with no sign until the report at exit. `kFrameArenaBytes` became 1 MB, about
  14,000 draw items at 72 bytes plus the lines. The first frame that falls to the heap prints one
  `[WARNING]` with its high water mark. The check fails on any `[WARNING]`, so an overflow cannot
  slip through.

## Rejected

- An `Input` phase: no system reads input through the world, and an empty phase only prints.
- Movement in Update: Fixed would have no system until 4c, and the accumulator would go untested.
- One accumulator per system: each system would take a different number of steps per frame.
- One playback at the end of the frame: a tag added in Update would stay invisible to Extract.
- A `SystemDesc` with all four facts, filled by the scene: the scene would restate each system's
  reads and writes, and those lines would drift from the code with no error.
- A `std::function` holding a lambda: a lambda that captures `this` reaches into the scene for its
  query and state, which breaks habit 1. A class keeps the state in one visible place.
- The non-const `Column` calling the const one through a `const_cast`: its safety rests on the
  reader, not the compiler. C++23's "deducing this" is the other route, and less familiar.
- Typed access that checks reads and writes, like Bevy's `Query<&Position, &mut Velocity>`: much
  template code for 4 systems.
- The three ECS classes in `include/core`: the ECS would leave its folder for two names, and the
  registry would have no test target.
- A resource store now (Bevy's resources, Mass's subsystems, Flecs's singletons): worth it once the
  context carries more outputs, such as audio events or UI.
- `Query` taking the world in `Matches(world)` and storing no pointer: its cache holds one world's
  archetypes, and a call with another world would return them with no error.
- One "Playback" span for all phases: the table could not say which phase's playback costs the time.
- The scene owning the buffer and passing it into each phase: every caller repeats the playback
  line, and a missing one compiles.
- One vector of phase and system pairs, or a `std::map` keyed by phase: every phase looks at every
  system, or a tree lookup for three fixed keys. Speed did not decide either: with 4 systems every
  option costs next to nothing (a guess, not measured).
- A float and a `while` loop in `Application::Run`: it works, but can only be checked by watching.
- An accumulator per scene: the load time still reaches it through `Application`'s `Timer`, and the
  frame loop would reach into the scene for its clock.
- `FixedStep` folded into `Timer`: one class with two jobs, and its tests would need SDL.
- Scene0-only systems fed through their constructors, such as `DamageSystem` copying 100 handles:
  it kept the benchmark exact by carrying Scene0's scaffolding into the systems.
- Each scene calling `Print` itself: a repeated line, and a scene that forgets prints nothing.
- A `DrawWorldReferenceSystem` in every scene: a repeated line, a scene that forgets has no grid,
  and the G toggle would need a way into each scene's system.
- The camera as a system: it is one plain object the scene owns, not world data, and a system would
  reach into the scene for it, against habit 1.
- Changing the math library to throw `std::runtime_error` or return zero: it is course code we
  copied in, and a silent zero would turn a broken normal into "nothing moves", found much later.
- A scene asking for its own frame arena size: an interface for one number each scene would still
  guess, and the arena would need rebuilding on every switch.
- Measuring Playback in Scene3: it lives on the uncommitted prototype branch, and autoplay makes
  every run different.

## What the build found

- A first `Print` had a separate `FindSharedWrites` that returned the pairs, so a test could check
  them. Vakarian dropped it: the note is only for reading, and the masks already hold the fact. The
  cost: only reading the print tests the check.
- The profiler moved into CMake's `ECS_SOURCES`, so `EcsTests` and `ArenaDemo` link it.
- `SystemContext`'s reference members `world` and `commands` carry a trailing `// NOLINT` for Rider's
  reference-member check, the one exception to the comment style.
- `FixedStep`'s tests ride in `EcsTests`, with `FixedStep.cpp` added to that target only. A 1 s stall
  gives exactly 15 steps, so float rounding did not cost a step. A `CoreTests` target waits until a
  second non-ECS piece needs tests.
- `DamageSystem` counts runs, not frames: the phase decides what a run is. Movement and Damage carry
  a "TEST SYSTEM" comment; they are Scene0's benchmark load.
- Scene0's world arena went from 256 KB to 512 KB. The first run asserted "the arena is full": each
  cube now passes through more archetypes, and each archetype reserves 1,000 rows. The world uses
  246,608 bytes.
- `SceneManager::ApplyPendingSwitch` returns `bool`, so `Application` knows when to reset
  `FixedStep` and restart the `Timer`.
- `SafeNormalize` sits in `include/core/VectorMath.h`. It compares against `VERY_SMALL`, the
  library's own threshold, so it never throws. The fly camera uses it.
- The frame arena warning goes to `std::cerr`, inside the loop.

## Measured

**Build 5, Debug, 11 s per scene.** Scene0: Movement made 612 calls in 627 frames. "Playback Update"
ran 10 times with 100 migrations each. No "Playback Fixed" or "Playback Extract" row. Scene1: Spin
made 584 calls in 598 frames. `MakeDrawList` in Scene0 averaged 0.536 ms, against 0.161 ms for the
old build: the rotation matrix and two more columns per cube. 20 switches: 21 closing reports, 0
leaked blocks, no `[WARNING]` or `[ERROR]`, 0 frame requests fell to the heap, peak live bytes
663,976.

**Build 6.** A forced `normalize` of a zero vector printed `[ERROR] math library: Divide by nearly
zero` and exited with code 1. With a 16 KB arena, the warning printed once: "High water 12504 of
16384 bytes". With 1 MB, 20 switch presses (19 landed; one was lost to window focus) gave 0 leaked
blocks, no `[WARNING]` or `[ERROR]`, 0 frame requests fell to the heap, and peak live bytes
1,581,480. That is 917,504 more than build 5, exactly the arena's growth from 128 KB to 1 MB.

**Build 7, 2026-10-26.** Release, Scene0, 12 s runs, alternating a build of `28bfe3c` (before step 3)
with the step 3 build, 5 runs each, run 0 of each dropped as cold. Means of runs 1 to 4, in ms per
frame:

| span | before | after | change |
|---|---|---|---|
| `Record` | 0.115 | 0.114 | -1%, inside the floor |
| `Scene::Update` | 0.0055 | 0.0070 | +27%, about 1.5 microseconds |
| `MakeDrawList` | 0.0118 | 0.0328 | +178%, about 21 microseconds |
| Playback, total per frame | about 0.0007 | 0.0006 | down |

- The profiler prints 3 decimals of a millisecond, so spans this small round by up to 0.0005 ms.
  Before's "Playback" row read 0.000 or 0.001 per frame; its estimate comes from the row total.
  After's comes from "Playback Update": 11 runs averaging 0.039 ms, over about 698 frames.
- `MakeDrawList` builds a rotation matrix per cube and reads two more columns, as expected.
- Movement made 681 calls in 698 frames. No "Playback Fixed" or "Playback Extract" row.
- The rule set before the runs: if Playback per frame rose more than 8%, ECS hardening item 2
  (create with components) would move ahead of physics. It went down, so hardening stayed after
  physics.

**The `Scene::Update` rise, split.** Two scratch worktrees, `-before` at `28bfe3c` and `-after` at
`eb21b14`, with the profiler printing microseconds and temporary scopes around the camera, the fixed
loop and the Update phase. Three sets of 4 counted runs each, in microseconds per frame:

| part | before | after |
|---|---|---|
| `Scene::Update` | 5.4 to 5.6 | 6.4 to 6.5 |
| camera | 0.9 | 0.9 to 1.0 |
| Movement, per call | 3.6 to 3.7 | 4.15 to 4.25 |
| the fixed loop around Movement (`FixedStep`, `RunPhase`, Movement's scope) | none | about 0.3 |
| Damage | 0.05 (11 frames) | 0.13 (every frame, mostly an early return) |
| playback | 0.5 (every frame) | 0.6 (11 frames, about 40 each) |

- Movement's own loop is about 0.5 microseconds (14%) slower per call, in every set. The rest of the
  rise is the registry's fixed cost: the fixed loop, Damage running every frame, and one profiler
  scope per system.
- Two guesses were tested and ruled out. Copying `boxHalfWidth` into a local, in case writes through
  `positions` forced a reload: 4.25 to 4.20, noise. Skipping empty archetypes, since the query now
  matches 5 instead of 2: 4.20 to 4.15, noise.
- The disassembly, from `/FAs` in both Release builds. Neither loop uses SIMD: both work one float
  at a time (`mulss`, `addss`, `comiss`). The old loop keeps 10, -10 and 20 in registers, because
  `kBoxHalfWidth` was a `constexpr`. The new loop reloads `boxHalfWidth` from the system object for
  every coordinate (`movss xmm2, [rsi+88]`), because the compiler cannot prove a write through
  `positions` leaves the member alone. The local copy gives a loop that matches the old one, but it
  timed the same, so those instructions are not where the 0.5 microseconds go.
- The cause is not found. It may be where the data sits in the cache when Movement runs; that would
  need CPU counters (VTune or AMD uProf) to confirm. Not pursued.

**Accepted by Vakarian.** `Record` held and Playback went down. The `Update` rise is about 1
microsecond per frame: half the registry's fixed cost, half outside Movement's instructions.
Movement keeps the local copy of `boxHalfWidth` as a hot-loop habit, though it did not change the
timing.
