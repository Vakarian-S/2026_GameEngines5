# Plan

**Last updated:** 2026-11-25.

This plan builds the Boss Fight Scene: a headless server runs a small boss fight, and clients connect
to it through Valve's GameNetworkingSockets (GNS). Joins and leaves go as reliable messages, input and
snapshots as unreliable ones, and clients draw the snapshots with interpolation. The server decides
everything, and there is no prediction. The goal is the engine's first networking: a headless mode
and a server and client split that later networked features and headless tests reuse. The course's
networking topic comes along with it. It came from ROADMAP.md, Networking, Boss Fight Scene row 1.

## Steps

| # | step | done when | stage |
|---|---|---|---|
| 1 | A headless mode: the engine starts with a server flag, opens no window, and the Fixed phase still ticks | A server run prints its tick count, opens no window, and closes clean | done (d58e448) |
| 2 | The Boss Fight Scene offline: a boss on a pattern, a player that moves and shoots, in one process | The scene plays in one window, with no network | done (cacd49e, 1d06049) |
| 3 | Join and leave over GNS: the server hands the client an ID, and spawns or despawns its player | Two clients join and leave, and the server's player count follows | designed |
| 4 | Input as unreliable messages: WASD on a client moves its player on the server | The server logs each player moving from its own client's keys | outline |
| 5 | Snapshots as unreliable messages, mirrored on the client through `NetworkIdMap` and drawn raw | Both clients show the boss and both players, with visible jitter | outline |
| 6 | Interpolation from a `SnapshotBuffer`, drawn about 100 ms in the past | The jitter from step 5 is gone | outline |
| 7 | Shooting: fire goes up as input, and the projectile comes back in a snapshot | A client fires, and both clients see the projectile | outline |
| 8 | A lag test: a stats line (ping, loss, snapshot age), a switch that sends snapshots as reliable messages, and GNS's fake loss at 5% | Under 5% loss, unreliable snapshots skip a frame and reliable ones freeze then jump | outline |

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

## 1. Headless mode

**The problem.** `Application` builds its `Window` and `VulkanRenderer` in its constructor, and each
scene's `OnCreate` loads meshes through the renderer's `AssetLoader`. A server has no window and no
renderer.

**Decisions.** Agreed 2026-11-20.

- **a. How the server starts.** One exe takes a `--server` flag. `main` reads it and builds either
  `Application` or a new `ServerApplication`, both in `app`, then calls `Run`. `main` stays the only
  file that includes `app`, and the build stays one target. Unity offers both ways: its player takes
  `-batchmode -nographics`, and it has a Server build subtarget
  ([command line](https://docs.unity.com/en-us/engine/current/manual/unity-editor/command-line-arguments/player),
  [dedicated server](https://docs.unity.com/en-us/engine/current/manual/platform-specific/dedicated-server/build)).
  A separate exe, as Unreal builds from a `<Project>Server.Target.cs`
  ([Unreal docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine)),
  was rejected: it needs the source glob split into libraries, and it only proves at link time that
  the server never links Vulkan. An optional window and renderer inside `Application` was rejected,
  because every line of the loop would branch on them.
- **b. Assets on the server.** A `NullAssetLoader` in `core` returns empty handles. `ServerApplication`
  owns one and hands it to its `SceneManager`; `Application` keeps handing over the renderer's
  `AssetStore`. The application picks the loader, so nothing checks at runtime. Every scene's
  `OnCreate` runs unchanged, and handles mean nothing on the server, because snapshots send a kind
  byte. A server-only path in each scene's `OnCreate` was rejected.
- **c. Phases.** The server runs only the Fixed phase, and one tick is one fixed step. Everything the
  server decides lives in Fixed; the client still runs Update and Extract each frame for its camera,
  input, interpolation and drawing. Extract would assert on the server anyway, because nothing sets
  `DrawListTarget`. Unity's Netcode for Entities splits it the same way: "the server always updates at
  a fixed time-step", 60 ticks a second by default, while the client "updates at a dynamic time step"
  and presentation systems are "only added to the client world"
  ([Unity docs](https://docs.unity3d.com/Packages/com.unity.netcode@1.3/manual/client-server-worlds.html)).
  Running Update at a free frame rate was rejected.
- **d. Pacing.** `ServerApplication` uses `FrameCap` at 60 per second to sleep until the next tick.
  With no FIFO present to wait on, a bare loop would spin a core (a guess, not measured). A spinning
  loop was rejected.
- **e. Quitting.** Ctrl+Q ends the loop, so the memory report and the leak check still print. Ctrl+Q is
  an ordinary key, not a signal, so `ServerApplication` reads the console's input buffer once per tick:
  `GetNumberOfConsoleInputEvents` gives the count of unread records
  ([docs](https://learn.microsoft.com/en-us/windows/console/getnumberofconsoleinputevents)), and
  `ReadConsoleInput` runs only when the count is above 0, because it waits for a record
  ([docs](https://learn.microsoft.com/en-us/windows/console/readconsoleinput)). A key record with
  `wVirtualKeyCode` `'Q'` and a Ctrl bit in `dwControlKeyState` stops the loop
  ([docs](https://learn.microsoft.com/en-us/windows/console/key-event-record-str)). The code lives in
  `ServerApplication`, so `core` never includes `windows.h`. Ctrl+C keeps Windows' default and kills the
  process with no reports: it still works when a bug hangs the loop, because Windows handles it
  outside the loop ([docs](https://learn.microsoft.com/en-us/windows/console/setconsolemode)). A Ctrl+C
  handler that sets a flag was rejected: it runs on a new thread
  ([docs](https://learn.microsoft.com/en-us/windows/console/handlerroutine)) and would take away the
  hard kill.
- **f. A tick limit.** `--ticks N` stops the server after N ticks, for scripted checks, which have no
  console to type Ctrl+Q in. Without the flag the server runs until Ctrl+Q. Removing it from a shipping
  build belongs to ROADMAP.md, Debug tools, A shipping build. Killing the process was rejected, because
  it skips the reports.
- **g. Which scene.** Each flag does one job: `--server` makes the run headless, `--ticks N` sets the
  lifespan, and `--scene N` picks the row. Changed 2026-11-20: Vakarian chose this over a bare number
  after `--server`, because it reads more clearly, though it is more verbose. The game takes `--scene N`
  too, in place of today's bare `argv[1]`, so one parser serves both applications and flags can come in
  any order. `PickScene` takes the text after `--scene`; empty text picks the last row, as no argument
  does today. Step 1 runs `--server --scene 3`, `PacingScene`, whose bottom row moves in Fixed. Once a
  second the server prints the tick count and one Mario's x, which shows the simulation runs with no
  window. A new empty test scene was rejected: it would only show that a loop runs. Rejected: the scene
  number right after `--server`, which gives one flag two jobs.
- **h. SDL time with no window.** `Timer` and `FrameCap` call `SDL_GetTicksNS`, and today only `Window`
  calls `SDL_Init`. SDL's docs do not say whether the clock needs it
  ([SDL_GetTicksNS](https://wiki.libsdl.org/SDL3/SDL_GetTicksNS)). The first build tests it; if the
  clock fails, `ServerApplication` calls `SDL_Init(0)`. **Settled 2026-11-20: the clock works without
  `SDL_Init`, so the server does not call it.** 600 ticks at a 60 per second cap took 10.02 s of wall
  time and 0.09 s of CPU (a `Stopwatch` around `GameEngines5.exe --server 3`, x64 Debug, and the
  process's `TotalProcessorTime`).
- **i. Win32.** The server stays in the x64 target. SDL3 and the engine sources build only on x64
  (CMakeLists.txt, the `GameEngines5` branch), and `Timer` needs SDL. Step 3 decides whether the `net`
  code gets a Win32 test target.
- **j. The check.** A script runs `GameEngines5.exe --server --scene 3 --ticks 600`. It passes with exit
  0, no window, "tick 600" printed, no `[WARNING]` or `[ERROR]`, and 0 leaked blocks.
  `scripts\run-game.ps1` gets a `-Server` switch that skips its window steps, and its `-Scene N` now
  passes `--scene N`.

**What was built.** Built 2026-11-20.

- `ServerApplication` in `include/app` and `src/app`. It owns a `NullAssetLoader`, a `SceneManager`,
  a `FrameCap` at `kTicksPerSecond` (60), `tickCounter` and `tickLifespan`. `Run` calls
  `RunPhase(Phase::Fixed, FixedStep::kFixedTimeStep)` and `Wait` once per tick, and stops at the tick
  limit or on Ctrl+Q (`IsCloseCommandTriggered`). Every 60 ticks it prints `tick N, mario x X` from
  the first chunk of a `Query` on `TransformComponent` and `FixedStepRowTag`.
- `Main.cpp`: `HasServerFlag` looks for `--server` in any argument and builds one application or the
  other in an `if` / `else`, so both fall through to the profiler, memory and leak reports.
- `NullAssetLoader` in `include/core/NullAssetLoader.h`, header only; each function returns `{}`.
- The scene table moved out of `Application.cpp` into `include/app/SceneIndex.h` and
  `src/app/SceneIndex.cpp`: `kScenes`, `Scenes()`, and `PickScene(argc, argv)`, which reads
  `--scene N` and picks the last row when the flag is missing. `Application.cpp` now includes
  `core/Camera.h` itself, because it lost the full `Camera` type when the scene includes left.
- `GetTickLifespan` in `ServerApplication.cpp` reads `--ticks N` and throws on a missing value, a
  non-number or 0.
- `windows.h` comes after every engine header in `ServerApplication.cpp`: it defines `near` and `far`
  as empty macros, which break `MMath.h`.
- `.run/GameEngines5 Server.run.xml`: a Rider configuration that runs the x64 Debug exe with
  `--server --scene 3` in an external console, so Ctrl+Q reaches it.
- `scripts\run-game.ps1`: `-Scene N` passes `--scene N`. `-Server` with `-Ticks N` (600 by default)
  passes `--server --ticks N`, skips the window steps, waits for the exit, and adds the window check,
  the tick lines and "Ran for" to the summary.

**What the build checked.** x64 Debug.

- **Build.** `cmake --build --preset x64-Debug` with no errors and no warnings.
- **Tests.** No tests were added: step 1 has no math or ECS code to test on its own.
  `scripts\run-tests.bat` passes in all four configurations.
- **The server,** through `scripts\run-game.ps1`:

  | run | result |
  |---|---|
  | `-Server -Scene 3 -Ticks 600` | exit 0, no window, 10 tick lines from tick 60 to tick 600, "Ran for 600 ticks", 0 `[WARNING]`, 0 `[ERROR]`, 0 asserts, 0 leaked blocks |
  | `-Server -Scene 0 -Ticks 60` | exit 0, no tick lines (Scene0 has no `FixedStepRowTag`), 0 leaked blocks |
  | `-Server -Scene 9 -Ticks 60` | exit 1, prints the scene list |

  The Mario x moves 10 units a second and wraps from 15 to -15 (-5, 5, -15, -5 ...), which matches
  `kSpeed` 10 and `kPathWidth` 30, so the Fixed phase runs at real-time speed.
- **`--ticks` by hand:** `--ticks 120` ran 2.0 s; `--ticks abc`, `--ticks 0` and a bare `--ticks` each
  printed their message and exited 1; with no `--ticks` the server still ran after 15 s.
- **Pacing (decision h):** 600 ticks took 10.02 s of wall time and 0.09 s of CPU.
- **Ctrl+Q:** Vakarian stopped the server with it in a terminal. In Rider's run window it does not
  work, because stdin is a pipe there; the run configuration uses an external console for that reason.
- **The game:** `scripts\run-game.ps1 -Scene 2` with F4, then F1, exits 0 and closes Physics Scene,
  Pacing Scene and Scene0 in turn, with 0 `[WARNING]`, 0 `[ERROR]`, 0 asserts and 0 leaked blocks.

## 2. The Boss Fight Scene offline

**The problem.** A fight that plays in one window, before any network exists. Its systems split into
server work and client work now, so step 3 only picks which lists run.

**Decisions.** Agreed 2026-11-20.

- **a. Scene slot.** A fifth scene, on F5; the app's F1 to F4 range grows to F5. The other scenes stay
  as benchmarks. Replacing one was rejected.
- **b. Camera.** A fixed overhead camera, angled down, with play on the XZ plane. WASD moves the
  player, so the fly camera's WASD would fight it. Keeping the fly camera was rejected.
- **c. Input.** A `PlayerCommandComponent` on each player entity holds a move direction (WASD) and
  an aim direction (the arrow keys), each normalized. `PlayerCommandSystem`, in Update, reads
  `InputState` and rewrites the local player's whole command every frame. It is the only writer of
  `PlayerCommandComponent`, and Fixed systems read only the command, never `InputState`. A held arrow aims and
  fires at once, as in a twin-stick shooter; there is no fire flag, so a shot can never have a zero
  direction. The gun's cooldown sets the rate, and also stops a second shot when one frame runs two
  fixed steps. Changed 2026-11-24: Space fired along the last move direction, and moving, aiming and
  firing needed three hands. In step 4 the server fills the same
  component from the network, and no Fixed system changes. A component, not a resource, because the
  server holds one command per player. Overwatch does the same: its input system fills a singleton
  input from the OS, and its command system "becomes the only place" that changes the `PlayerCommandComponent`
  that is networked and simulated (GDC 2017, "Overwatch Gameplay Architecture and Netcode", 13:52 to
  14:56). Rejected: Fixed systems that read `InputState`, and a fire latch that a Fixed system clears,
  because it adds a second writer.
- **d. Movement.** A player system turns the command into a `PHYSICS::VelocityComponent`, the existing
  `UpdatePositionSystem` moves the player, and a clamp keeps it inside the arena. It reuses the Fixed
  system `PhysicsScene` runs. A second integrator was rejected.
- **e. A tick counter.** A `FixedTickCount` resource counts fixed steps. `Application` and
  `ServerApplication` add 1 and `Set` it before each Fixed run. The boss pattern, step 1's printout and
  step 5's snapshot header read it. It is ROADMAP.md, Fixed step additions row 3. Each user counting on
  its own was rejected.
- **f. The boss.** Its position is a free function of the tick, a figure-eight on the plane. It holds
  no state, and a test checks it with no ECS (rule 9). The boss does not attack in this step: nothing
  in the outline needs it. A pattern stepped by delta time was rejected, because it drifts.
- **g. Shooting.** `ShootSystem`, in Fixed, spawns a projectile through the command buffer (rule 6)
  while the command's aim is not zero and the tick has reached the gun's `readyTick`. The projectile
  gets `TransformComponent`, `VelocityComponent` along the aim, `LifespanComponent { ticksLeft }` and a `RenderableComponent`.
  The player's `GunComponent` in `game/Components.h` holds `readyTick` and the stats: projectile
  speed, lifespan ticks, cooldown ticks and the projectile's `RenderableComponent`. Stats on the component let
  a second weapon be a different `GunComponent{}`, with no new system; one component passes ECS.md
  rule 3, because only `ShootSystem` reads it. `LifespanComponent` lives in `core/Components.h`, and
  `LifespanSystem` in `core/systems` counts it down in Fixed and destroys the entity at 0 through the
  command buffer, so a hit spark or a pickup can reuse it. A projectile also despawns on a hit.
  Rejected: aiming with the mouse, which needs a ray to the floor; stats as `ShootSystem` constants,
  which allow one weapon only; a shared weapon table, which pays off only when many entities share
  a weapon; splitting `GunComponent` into state and stats, which pays off only with many guns; and a
  `Projectile` component in `game`.
- **h. Hits.** The boss and each projectile get a `SphereColliderComponent`. The test lives in
  `include/physics/Collision3D.h` and `src/physics/Collision3D.cpp`, free functions in `PHYSICS` next to
  `RayIntersect.h`. This step adds `Overlap(TransformComponent, SphereColliderComponent, TransformComponent, SphereColliderComponent)`,
  which returns a bool, with each radius times the scale (rule 10 keeps spheres uniform). Collision
  row 1 later adds the other shape pairs and a `Collide(...)` that returns a contact; resolving stays
  in `RigidBody.h`. `physics` may include `core`, and `game` may include `physics`, so both reach it.
  On a hit, the boss's `HealthComponent` drops by the projectile's `DamageComponent` and the projectile
  despawns. `ProjectileHitSystem`, in Fixed, pairs entities with `DamageComponent` against entities
  with `HealthComponent`; a `Damage` component, not a `Projectile` tag, marks what can hit, and the
  amount comes from the gun's stats. Projectiles are the outer loop and a projectile stops at its
  first hit, so it deals damage once even though its destroy waits for the end of the phase. Health
  and damage are `int32`, because health is subtracted and can pass below 0. `Collision2D.h` waits for its
  first caller, because nothing has a 2D collider. Rejected: `game` as the folder, `RayIntersect.h` as
  the file, waiting for Collision row 1, and an empty `Collision2D.h`.
- **i. The boss dies.** At 0 health the boss despawns and the console prints "boss died". A
  `BossDeathSystem` on `BossTag + HealthComponent` does both, in Fixed after
  `ProjectileHitSystem`, so hits only change health (rule 8) and death is a scan, not a callback
  (ECS.md rule 7). A general death system waits until something other than the boss has health. R
  restarts the fight with `World::Clear`, as `PhysicsScene` does. The scene loads its assets once in
  `OnCreate` and keeps three `RenderableComponent` members, so `SpawnEntities` needs no asset loader. It is
  the rare event a reliable message carries later.
- **j. Server and client systems.** Pattern, movement, shooting, lifespan and hits go in Fixed, the server's
  list. `PlayerCommandSystem` goes in Update and drawing in Extract, the client's list. Step 3 then
  picks lists by role with no system rewritten.
- **k. Assets.** Mario for the player, the Triceratops with its palette for the boss, a small cube for
  projectiles, a flat scaled cube for the floor. All exist. New models were rejected. The floor is
  spawned in `SpawnEntities`, so it survives R, with `floor_grey.png` and no collider.
- **l. The check.** Tests: the boss pattern at known ticks in `PhysicsTests`, `LifespanSystem` in
  `EcsTests`, and the sphere `Overlap` in `PhysicsTests`. `scripts\run-game.ps1 -Scene 4` holds WASD and the arrow keys; the
  screenshots show the player moving and projectiles in flight, the console shows "boss died", and the
  run ends with 0 leaked blocks.

**For step 4,** from the same talk: the server keeps a small buffer of unsimulated inputs and reuses
the last one when an input is lost (transcript lines 674 to 689), and the client sends every input
since the server's last acknowledgment in each packet, so one lost packet costs nothing (lines 728 to
746).

**What the build checked.** x64 Debug, one entry per decision as it lands.

- **Decision f, 2026-11-21.** `FigureEightPathPosition(tick, periodTicks, halfWidth, halfDepth)` in
  `game/FigureEightPath.h` gives the offset. A `FigureEightPathComponent` holds each entity's
  `center`, period and sizes, so several entities can loop in different spots.
  `FigureEightPathSystem` (it was `BossPatternSystem`) writes `center + offset` in Fixed. The boss
  uses center (0, 0, -3), period 960, half width 6, half depth 2. Mario starts at (0, 0, 5).
  - **Tests.** `PhysicsTests` gets "a figure-eight path loops through the origin", with 8 checks:
    ticks 0, half a period and a whole period give the origin; an eighth gives
    (0.707 × halfWidth, 0, halfDepth); a quarter gives (halfWidth, 0, 0); three quarters give
    (-halfWidth, 0, 0); 1000 periods later gives the same point; y stays 0.
    `src/game/FigureEightPath.cpp` joins the `PhysicsTests` sources. `scripts\run-tests.bat` passes
    in all four configurations.
  - **The game.** `scripts\run-game.ps1 -Scene 4`: screenshots 2 s apart show the boss moving from
    the right of its center to the left, clear of Mario. F1, then F5, restarts the boss near the
    start of its path, so the `FixedTickCount` reset works. Exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0 leaked
    blocks.
  - **The server.** `-Server -Scene 4 -Ticks 180`: exit 0, no window, "Ran for 180 ticks", 0 leaked
    blocks.
- **Decision g, 2026-11-24.** `PlayerCommandComponent` holds `moveDirection` and `aimDirection`, with no fire
  flag. `ShootSystem` spawns a cube along the aim while the aim is not zero, at most once every
  `cooldownTicks`. `GunComponent` holds `readyTick` and the stats; the scene sets speed 15 m/s,
  lifespan 90 ticks and cooldown 12 ticks. `LifespanSystem` destroys an entity when
  `LifespanComponent::ticksLeft` reaches 0. Both run in Fixed after `ClampToArenaSystem`.
  - **Tests.** `EcsTests` gets "LifespanSystem", with 6 checks: `ticksLeft` 3 is alive after 2 runs,
    each run takes 1 tick, and it is gone after the 3rd; `ticksLeft` 5 is alive then and gone after
    the 5th; an entity with no lifespan stays. `EcsTests` now compiles `LifespanSystem.cpp` and links
    `MathLibrary`, because `core/Components.h` needs the math headers. `scripts\run-tests.bat` passes
    in all four configurations; `EcsTests` went from 376 to 382 checks.
  - **The script.** `scripts\run-game.ps1` gets Up, Down, Left and Right. Arrows are extended keys, so
    their codes carry 0xE0 and the script sends them with the extended-key flag.
  - **The game.** `scripts\run-game.ps1 -Scene 4`: Up held shows a line of cubes about 3 m apart,
    which matches 15 m/s at 5 shots a second; D with Right held moves Mario right and fires right;
    2 s with nothing held removes every cube; Up with Left fires along the diagonal. Exit 0,
    0 `[WARNING]`, 0 `[ERROR]`, 0 leaked blocks.
  - **The server.** `-Server -Scene 4 -Ticks 180`: exit 0, no window, "Ran for 180 ticks", 0 leaked
    blocks.
- **Decision h, 2026-11-24.** `PHYSICS::Overlap` in `physics/Collision3D` compares the distance
  between two centres with the sum of `radius * scale.x`. Projectiles carry `SphereColliderComponent` and
  `DamageComponent`; the boss carries `SphereColliderComponent` (radius 0.5, so 1.5 m at scale 3) and
  `HealthComponent` 20. `ProjectileHitSystem` runs in Fixed after `LifespanSystem`, prints
  "target health N" on each hit for now, and destroys the projectile.
  - **Bugs the first run found.** The boss started at 0 health, and unsigned health wrapped from 0 to
    4294967295 on the first hit and could never reach 0 again; health and damage became `int32`.
    `Overlap` added the scale to the radius instead of multiplying, so hits landed 6 m away.
  - **Tests.** `PhysicsTests` gets "two spheres overlap", with 9 checks: 0.9 apart overlaps, 1.1 apart
    does not, exactly touching does not count, one inside the other and the same centre overlap, the
    order of the spheres does not matter, scale 3 reaches 1.9 but not 2.1, and the distance counts
    every axis. `src/physics/Collision3D.cpp` joins the `PhysicsTests` sources.
    `scripts\run-tests.bat` passes in all four configurations; `PhysicsTests` went from 226 to 235
    checks.
  - **The game.** `scripts\run-game.ps1 -Scene 4`, Up held for 6 s: health printed 19 down to 10, one
    point per hit, and cubes that miss fly past the boss. Exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0
    leaked blocks. Holding Up, then Up and Right, fired every cube along the diagonal; a side shot
    Vakarian saw by hand did not happen with the script's keys.
  - **The server.** `-Server -Scene 4 -Ticks 180`: exit 0, no window, "Ran for 180 ticks", 0 leaked
    blocks.
- **Decision i, 2026-11-24.** `BossDeathSystem` prints "boss died" and destroys a boss at 0 health or
  below. `ProjectileHitSystem` no longer destroys targets. `BossFightScene::SpawnEntities` creates
  the player and the boss from the stored `RenderableComponent` members, and `Update` calls `world.Clear()`
  then `SpawnEntities()` when R is pressed.
  - **Tests.** `EcsTests` gets "BossDeathSystem", with 4 checks: a boss at 0 health and a boss at -3
    are gone after one run, a boss at 1 health stays, and an entity at 0 health without the boss tag
    stays. `src/game/systems/BossDeathSystem.cpp` joins the `EcsTests` sources.
    `scripts\run-tests.bat` passes in all four configurations; `EcsTests` went from 382 to 386 checks.
  - **The game.** `scripts\run-game.ps1 -Scene 4`, Up held for 20 s: the boss crosses the line of fire
    only part of the time, so 20 hits take about 18 s. The boss vanished and "boss died" printed
    once. R brought Mario and the boss back, the boss at the current tick's point on its path. Exit 0,
    0 `[WARNING]`, 0 `[ERROR]`, 0 asserts, 0 leaked blocks.
  - **The server.** `-Server -Scene 4 -Ticks 180`: exit 0, no window, "Ran for 180 ticks", 0 leaked
    blocks.
- **Decision j, 2026-11-24.** Nothing to build. `BossFightScene::OnCreate` already puts every
  simulation system in Fixed (path, movement, position, clamp, shoot, lifespan, hits, death),
  `PlayerCommandSystem` alone in Update, and the two draw systems in Extract. `ServerApplication`
  runs only Fixed, so it simulates the whole fight. R in `BossFightScene::Update` is the one
  client-only piece outside the lists; step 3 turns it into a reliable message.
- **Decision k, 2026-11-24.** A floor entity with `TransformComponent` and `RenderableComponent` only: the cube mesh,
  `floor_grey.png`, scaled to the arena (20 × 0.1 × 14 m) and placed at y = -1.5 so Mario's feet sit
  on it.
  - **A crash the floor caused.** The first shot after adding the floor stopped the game with "the
    arena is full, raise its size". Each archetype takes at least one 16 KB chunk; floor, player,
    boss and projectile made four, 64 KB, which was all of `kWorldArenaBytes`. It is now 256 KB.
  - **The game.** `scripts\run-game.ps1 -Scene 4`: D for 3 s, then Up: the screenshot shows the grey
    floor, Mario clamped at its right edge, and cubes in flight. R brings the floor back with Mario
    and the boss. Exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0 asserts, 0 leaked blocks, no world
    allocation fell to the heap.
  - **The server.** `-Server -Scene 4 -Ticks 180`: exit 0, no window, "Ran for 180 ticks", 0 leaked
    blocks.
- **Decision l, 2026-11-24: the step's final check,** on the finished code.
  - **Tests.** `scripts\run-tests.bat` passes in all four configurations: `EcsTests` 386 checks
    (`LifespanSystem`, `BossDeathSystem`), `PhysicsTests` 235 (`FigureEightPathPosition`, `Overlap`).
  - **The game.** One `scripts\run-game.ps1 -Scene 4` run: A and W move Mario up and left; D and S bring
    him back; Up held for 23.5 s shows cubes in flight, hits the boss as it crosses, and the console
    prints "boss died" once, with the boss gone from the screenshot; R brings back the floor, Mario
    and the boss. Exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0 asserts, 0 leaked blocks, no frame or world
    allocation fell to the heap.
  - **The server.** `-Server -Scene 4 -Ticks 600`: exit 0, no window, "Ran for 600 ticks", 0 leaked
    blocks.

## 3. Join and leave over GNS

**The problem.** Every copy of the game runs alone. This step lets the server know who is connected:
a client starts, finds the server and says hello; the server gives it a number and spawns a player
for it; when the client quits, the server removes that player. Movement and drawing over the network
come in steps 4 and 5.

**The library.** Decided 2026-11-20, after a prototype
(findings/2026-11-gns-prototype.md). Raw Winsock and SDL_net 3.2.0 were weighed first. GNS won
because it does the transport work a shipped game needs: whole messages, reliable and unreliable on
one connection, resends, encryption, connection stats and fake lag and loss. Its costs, from the
prototype: about 7.6 min on a machine's first cold configure (mostly vcpkg building OpenSSL, protobuf
and Abseil), about 750 MB of installed libraries per platform on the build machine, and about 13.6 MB
of Release DLLs next to the exe with the default vcpkg settings. Win32 builds with no source changes.
Starting with Winsock and swapping later was rejected: the TCP and UDP split would shape steps 3, 4 and
8, and a swap would throw that code away.

**One fact shapes most decisions.** GNS is one object per program. `GameNetworkingSockets_Init`
asserts on a second call (`src/steamnetworkingsockets/clientlib/csteamnetworkingsockets.cpp`, line
2506, at v1.6.1), and `RunCallbacks()` runs the callbacks for every connection in the program.

**Decisions.** Agreed 2026-11-20. Some depend on the first build, and change if it says otherwise.

- **a. How GNS is built.** FetchContent pulls GNS at tag `v1.6.1` and builds its static lib with
  `USE_CRYPTO=BCrypt`, as the engine fetches SDL. Our own `vcpkg.json` lists only `protobuf`, so
  OpenSSL is never built, which saves about 4.2 min per cold x64 configure. vcpkg's GNS package was
  rejected: it lists OpenSSL as a hard dependency.
- **b. Where vcpkg comes from.** Changed 2026-11-24 by the first build. vcpkg is the copy bundled
  with Visual Studio Build Tools 2026 (the "vcpkg package manager" component), and `VCPKG_ROOT`
  points at it, as `VULKAN_SDK` points at the Vulkan SDK. Only the x64 preset sets its toolchain
  file, and `CMakeLists.txt` stops with a clear error when it is missing. Both presets move to the
  `Visual Studio 18 2026` generator and the `v145` toolset, because vcpkg builds protobuf with the
  newest compiler on the machine, and the engine must use the same one. A contributor installs
  Build Tools 2026 with the C++ workload and vcpkg. A separate vcpkg clone was tried and dropped:
  it built with `v145` while the engine used `v143`, and Rider loaded VS's environment, so Rider
  and the terminal used different vcpkg copies. A git submodule was rejected: it adds 181 MB to
  every clone.
- **c. The two extra DLLs.** Settled 2026-11-24 by the build: the x64 preset sets both
  `VCPKG_TARGET_TRIPLET` and `VCPKG_HOST_TRIPLET` to `x64-windows-static-md`, so protobuf and
  Abseil link into the exe and no DLL of theirs ships. The host triplet matches because protobuf
  needs itself as a host tool (`protoc`); a different host triplet builds protobuf twice.
  `set(Protobuf_USE_STATIC_LIBS ON)` goes with the GNS options: with it off, GNS adds
  `PROTOBUF_USE_DLLS` (its `CMakeLists.txt` lines 210 to 211) and cannot link static protobuf.
- **d. The `net` module.** `include/net` and `src/net`, with the standard library and the GNS headers
  only. Those headers pull in no `windows.h` and no protobuf. `game`, `scenes` and `app` may include
  `net`; `core`, `physics` and `ecs` never do. `scenes` joined the list on 2026-11-25, because a
  scene inserts the `ClientTable` resource (decision l). Rule 2 gains that line. `ByteWriter`,
  `ByteReader` and `net/Components.h` include no GNS header, so targets without GNS can use them.
  The folders arrive with decision e's first file.
- **e. One GNS per program.** A `NetworkLibrary` in `net` calls `GameNetworkingSockets_Init` in its
  constructor and `GameNetworkingSockets_Kill` in its destructor. `Application` or `ServerApplication`
  owns it. Rule 12 gets a written exception: the library lives per program, and every connection, poll
  group and table lives per World. A World resource was rejected: a listen server's second World would
  call `Init` again.
- **f. Events to the right World.** The application calls `RunCallbacks()` once per frame, before the
  phases. One callback in `net` copies each status change into the `NetworkLibrary`'s event list; a
  callback is a plain function with no context pointer, so it reaches the library through one
  documented static pointer. Each World's system takes only the events for its own listen socket or
  connection, using `m_hConn` and `m_info.m_hListenSocket` (`isteamnetworkingsockets.h` line 1024,
  `steamnetworkingtypes.h` line 688). One callback per World was rejected: GNS gives no way to tell
  them apart.
- **g. GNS's own thread stays on.** Its default service thread does the socket work, and our thread
  only reads results. `MemoryMonitor` already locks around its counters, so counting from two threads
  is safe; its stats include that thread's allocations. Manual poll mode was rejected: its functions
  are in no public header.
- **h. Receiving.** The server puts its connections in one poll group per World and reads it with
  `ReceiveMessagesOnPollGroup` in Fixed. The client reads its connection with
  `ReceiveMessagesOnConnection` in Update.
- **i. Command line and role.** `--server --scene 4 [--port P]` and `--connect host[:P] --scene 4`, one
  flag per job as step 1, decision g sets, and one default port in
  `net`. `NetworkSettings { role, host, port }` lives in `core`, with role Offline, Server or Client.
  `SceneManager` takes it in its constructor and `Set`s it on each new World before `OnCreate`, and the
  scene picks its system lists from it. No flag means Offline, so step 2's fight still runs. Changing
  `OnCreate` in all five scenes was rejected.
- **j. Messages.** The first byte is the type; `ByteWriter` and `ByteReader` in `net` write the fields
  one by one in big-endian order, and the reader checks bounds. These go reliable: `Hello {
  protocolVersion }`, `Welcome { clientId, playerNetworkId }`, `PlayerJoined` and `PlayerLeft {
  clientId }`. GNS delivers whole messages, so there is no length prefix. A `memcpy` of a struct with
  padding was rejected.
- **k. Joining and leaving.** On Connecting at the listen socket, the server calls `AcceptConnection`,
  or `CloseConnection` when all 8 slots are taken. On Connected, it spawns the player through the
  command buffer and sends `Welcome`. On ClosedByPeer or ProblemDetectedLocally, it despawns the player,
  closes the handle and frees the slot.
- **l. `ClientTable` and `NetworkId`.** `ClientTable` is a server resource: a vector of slots, each
  with an `HSteamNetConnection`, a `clientId` and the player entity, at most 8. IDs come from a counter
  and are never reused within a run. `NetworkId` is a `u32` component in `net/Components.h`, given out
  by a server counter, on players for now. `NetworkIdMap` waits for step 5.
- **m. Connecting from the client.** `ConnectByIPAddress` does not block. The client prints
  "connecting" until the Connected event. On ProblemDetectedLocally it prints the reason and quits with
  an error.
- **n. Systems.** `AcceptClientsSystem`, `ServerReceiveSystem` and `ClientReceiveSystem`, in
  `game/systems`.
- **o. Tests.** The `ByteWriter` and `ByteReader` round trip goes in a target with the standard library
  only, so it runs on Win32 too. A `NetTests` target, x64 only, holds a server World and a client World
  in one program: it checks `Hello` and `Welcome`, and that each event reaches the right World. The
  Win32 configure stays free of network access.
- **p. The check.** A script starts the server, then client A, then client B, closes A, then B. The
  server prints player counts 1, 2, 1, 0, B prints "player A left", and every process exits with no
  `[WARNING]` or `[ERROR]` and 0 leaked blocks.

**What the build checked.** x64 Debug and Release, one entry per decision as it lands.

- **Decision a, 2026-11-24.** `vcpkg.json` at the repo root lists only `protobuf`, with
  `builtin-baseline` at the prototype's vcpkg commit `74f5ea45`. In the x64 branch of
  `CMakeLists.txt`, FetchContent pulls GNS `v1.6.1` with `SYSTEM`, after SDL. `USE_CRYPTO` is set
  to `BCrypt` in the cache with `FORCE`: GNS declares it with `set(CACHE)` under CMake 3.15
  policies, so a plain `set` loses on the first configure. `BUILD_SHARED_LIB`, `BUILD_EXAMPLES`,
  `BUILD_TESTS` and `BUILD_TOOLS` are off, and `ENABLE_ICE` keeps its default, on. `GameEngines5`
  links `GameNetworkingSockets::static`.
  - **The 3a result: OpenSSL is skipped.** A cold configure into `build/check-3a`, with an empty
    `VCPKG_DEFAULT_BINARY_CACHE`, the prototype's vcpkg as the toolchain and the `x64-windows`
    triplet, took 323 s (`Measure-Command { cmake -S . -B build\check-3a ... }`). The prototype's
    took 454 s. vcpkg built protobuf in 2.8 min, Abseil in 52 s and utf8-range in 8.9 s, from its
    "Elapsed time to handle" lines. The rest, about 94 s, is the SDL and GNS clones and CMake's
    checks. The log has no line with "openssl", and GNS prints
    `Crypto library for AES/SHA256: BCrypt`.
  - **The build.** Debug in 111 s and Release in 106 s, both from cold, with 0 compiler warnings.
    `GameNetworkingSockets_s.lib` is 27.5 MB in Debug and 10.7 MB in Release.
    `dumpbin /dependents GameEngines5.exe` lists no protobuf or Abseil DLL. Nothing calls GNS yet,
    so the linker leaves it out, and 3c waits for the first call, at decision e.
  - **Tests.** `ctest` on `build/check-3a` passes all 5 suites in x64 Debug and Release.
    `scripts\run-tests.bat` passes in Win32 Debug and Release; that configure takes 18.7 s and
    never runs vcpkg.
  - **The game.** `scripts\run-game.ps1 -Scene 0`, F2 to F5: the last screenshot shows the Boss
    Fight Scene. Exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0 asserts, 0 leaked blocks.
  - **The server.** `-Server -Scene 4 -Ticks 300`: exit 0, no window, "Ran for 300 ticks", 0 leaked
    blocks.
  - **Until decision b.** `build/cmake-x64` has no vcpkg toolchain, so its next configure fails to
    find protobuf.
- **Decision b, 2026-11-24.** `VCPKG_ROOT` is
  `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\vcpkg`, vcpkg tool 2026-07-27.
  The x64 preset sets `toolchainFile` to `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` and
  `VCPKG_TARGET_TRIPLET` to `x64-windows`. Both presets use `Visual Studio 18 2026` and `v145`.
  `CMakeLists.txt` stops with "vcpkg was not loaded" when `VCPKG_TOOLCHAIN` is not set.
  - **The missing toolchain.** A configure with no toolchain stops at `CMakeLists.txt:139` with that
    message.
  - **Bugs the first runs found.** A vcpkg clone in `D:\dev\vcpkg` built protobuf with MSVC 19.51
    (`v145`), while the engine built with 19.43 (`v143`). Rider loads VS 2022's environment, which
    set `VCPKG_ROOT` to VS 2022's bundled vcpkg, so Rider configured `build/cmake-x64` with a
    second vcpkg. Rider also configures that folder on its own, and twice it ran at the same time
    as a terminal configure: GNS's BCrypt test then linked with `-lbcrypt.lib` and failed, and once
    the GNS clone failed. The same configure in a folder Rider does not use passed.
  - **The configure.** Cold, in a fresh folder with VS 2026 and the bundled vcpkg: 264 s. vcpkg
    built Abseil in 28 s and protobuf in 2 min; a new compiler means no cache hits. Protobuf's build
    logs name only `MSVC/14.51.36231`. `build/cmake-x64` with protobuf already installed: 49 s. A
    `--fresh` configure on a tree with every package installed: 23.5 s.
  - **The build.** x64 Debug in 160 s and Release in 159 s, 0 warnings, 0 errors.
  - **Tests.** `scripts\run-tests.bat` passes in all four configurations; `GpuTests` 9 of 9 on x64.
  - **The demo output.** `scripts/capture-output.sh v145`: all four outputs match `baselines/vulkan`
    line for line, so the `v143` to `v145` move changed nothing the demo prints.
  - **The game.** `scripts\run-game.ps1 -Scene 0`, F2 to F5: the last screenshot shows the Boss
    Fight Scene at 240 fps. Exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0 asserts, 0 leaked blocks.
  - **The server.** `-Server -Scene 4 -Ticks 300`: exit 0, no window, "Ran for 300 ticks", 0 leaked
    blocks.
- **Decision c, 2026-11-24.** The x64 preset sets `VCPKG_TARGET_TRIPLET` and `VCPKG_HOST_TRIPLET` to
  `x64-windows-static-md`, and `CMakeLists.txt` sets `Protobuf_USE_STATIC_LIBS` to `ON` with the
  other GNS options.
  - **The 3c result: no protobuf or Abseil DLL ships.** `vcpkg_installed/x64-windows-static-md` has
    no DLL in `bin` or `debug/bin`, and holds `libprotobuf.lib` (231 MB) and 91 Abseil libs. vcpkg
    built protobuf once, in 2.6 min in all.
  - **A bug the first probe found.** With `Protobuf_USE_STATIC_LIBS` off, GNS compiled with
    `PROTOBUF_USE_DLLS`, and a program calling GNS failed to link: 30 unresolved
    `__declspec(dllimport) google::protobuf::...` symbols. With it on, GNS's project file has no
    `PROTOBUF_USE_DLLS`.
  - **The probe.** `GameEngines5` does not call GNS yet, so a scratch program outside the repo
    links the tree's `GameNetworkingSockets_s.lib` with static protobuf and Abseil, calls
    `GameNetworkingSockets_Init`, opens a listen socket on port 27099, closes it and calls
    `GameNetworkingSockets_Kill`. Debug (8.4 MB) and Release (2.1 MB) print "init ok, listen socket
    made" and "kill ok" and exit 0. `dumpbin /dependents` lists only Windows and C++ runtime DLLs.
  - **The build.** x64 Debug in 50 s and Release in 46 s after the change, 0 warnings, 0 errors.
  - **Tests.** `scripts\run-tests.bat` passes in all four configurations.
  - **The game.** `scripts\run-game.ps1 -Scene 0`, F2 to F5: exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0
    asserts, 0 leaked blocks.
  - **The server.** `-Server -Scene 4 -Ticks 300`: exit 0, no window, "Ran for 300 ticks", 0 leaked
    blocks.
- **Decision d, 2026-11-25.** Nothing to compile. `include/net` and `src/net` exist, empty until
  decision e's first file; git tracks them from then. `scenes` joined the modules that may include
  `net`.
  - **The include check.** `scripts\check-includes.ps1` reads every `#include` under `include/`,
    `src/` and `Main.cpp` against three tables: the modules each module may include, the modules
    that may include `<steam/...>` (`net`) and `<vulkan/...>` (`vulkan`), and the files that must
    include neither (`ByteWriter`, `ByteReader`, `net/Components.h`). It prints each break as
    file:line and exits 1. The tables came from today's includes, so `utils` and `ecs` may include
    each other: `Profiler.h` includes `ecs/WorldFrameStats.h`. It is not in `run-tests.bat` yet.
  - **Tests.** On the repo: 184 files, exit 0. On a scratch tree with four planted breaks (`core`
    including `net`, `physics` including `<steam/...>` and `game`, `<steam/...>` in `ByteWriter.h`):
    all four reported, the allowed includes passed, exit 1.
- **Decision e, 2026-11-25.** `NET::NetworkLibrary` in `net/NetworkLibrary.h` and `.cpp` calls
  `GameNetworkingSockets_Init(nullptr, ...)` in its constructor, throws `std::runtime_error` with
  GNS's message when it fails, and calls `GameNetworkingSockets_Kill` in its destructor. It cannot
  be copied or moved, and its header includes no GNS header. `Application` and `ServerApplication`
  hold one as their first member, so it starts before the scenes and stops after them. Offline
  runs start GNS too, until decision i adds the role.
  - **GNS in the exe.** `dumpbin /dependents GameEngines5.exe` now lists `WS2_32.dll`,
    `IPHLPAPI.DLL` and `bcrypt.dll`, and still no protobuf or Abseil DLL: 3c holds in the real exe.
    The Release exe grew from 0.67 MB to 2.7 MB.
  - **A leak report the first run found.** The Debug game and server reported 3 leaked blocks,
    848 bytes (8, 504 and 336). A scratch probe with only `Init` and `Kill` left the same 848
    bytes, and three rounds left no more. A second probe recorded a call stack per allocation:
    the blocks are the buffers of three global `std::vector`s in GNS's `csteamnetworkingsockets.cpp`,
    `s_vecConfigValueTable` (line 145), `s_vecConnectionConfigValueTable` (line 146) and
    `CSteamNetworkingSockets::s_vecSteamNetworkingSocketsInstances` (line 425). A global's
    destructor runs after `main` returns, and the leak report ran at the end of `main`.
  - **The fix: the leak report runs at process exit.** `Main.cpp` starts with
    `#pragma init_seg(lib)` (warning 4073 turned off), so its globals are built before the
    program's and GNS's and destroyed after them. `LeakReportAtExit` holds the baseline that `main`
    sets, and its destructor calls `MemoryMonitor::ReportLeaks`. `MemoryMonitor::Report` stays at
    the end of `main` and still shows 848 live bytes. A scratch probe tried it first: 0 leaked
    blocks after `Init` and `Kill`.
  - **`[ERROR]` on exceptions.** `main`'s `std::exception` catch prints `[ERROR] ` before the
    message, so `run-game.ps1` counts it. `--server --scene 99` printed `[ERROR] no scene "99"`,
    exited 1, and the leak report still ran, with 0 leaked blocks.
  - **The build.** x64 Debug and Release, 0 warnings, 0 errors. `check-includes.ps1`: 186 files,
    exit 0.
  - **Tests.** `scripts\run-tests.bat` passes in all four configurations.
  - **The game.** `scripts\run-game.ps1 -Scene 0`, F2 to F5, Debug: exit 0, 0 `[WARNING]`, 0
    `[ERROR]`, 0 asserts, 0 leaked blocks. `-Scene 4`, Release: exit 0, 0 `[WARNING]`, 0 `[ERROR]`.
  - **The server.** `-Server -Scene 4 -Ticks 300`, Debug and Release: exit 0, no window, "Ran for
    300 ticks", 0 leaked blocks in Debug.
- **Decision f, 2026-11-25.** `net/ConnectionEvents.h` holds `NET::ConnectionState` (None,
  Connecting, Connected, ClosedByPeer, ProblemDetectedLocally, Other), `NET::ConnectionEvent`
  (connection and listen socket as `std::uint32_t`, old and new state, end reason, a 128-char end
  message) and `NET::ConnectionEventQueue`, all with no GNS header. `NetworkLibrary.cpp` registers
  `OnStatusChanged` with `SetGlobalCallback_SteamNetConnectionStatusChanged`; the callback copies
  each change into the library's list through `statusEvents`, a pointer in the file's anonymous
  namespace, set after `Init` and cleared after `Kill`. `PollConnectionEvents()` clears the list,
  calls `RunCallbacks()` and returns a span. `Application::Run` and `ServerApplication::Run` call it
  once per frame, before the phases, and append the events to the current World's
  `ConnectionEventQueue` only when the World holds one. The World's system empties it, so a frame
  with 0 Fixed steps loses no event.
  - **A warning the first build found.** `ToConnectionState` listed every GNS state with no return
    after the switch: C4715 in both builds. A return of `Other` after the switch fixed it.
  - **The events test.** A scratch program compiles `NetworkLibrary.cpp`, opens a listen socket on
    port 27098 and connects to it over loopback in one process. 12 of 12 checks: the server side
    gets Connecting on its listen socket (65536), then Connected after `AcceptConnection`; the
    client side's events carry listen socket 0; after the client closes with reason 1000 and "client
    says goodbye", the server side gets ClosedByPeer with that reason and text; a poll with nothing
    new returns no events. Decision o moves it into `NetTests`.
  - **Decision g rides on this entry: nothing to build.** GNS's service thread stays on. A scratch
    test with `NetworkLibrary.cpp` and the engine's `MemoryMonitor.cpp` counted the process's
    threads with a Toolhelp snapshot: 4 before `Init`, 5 after `Init`, still 5 with a listen socket
    and a loopback connection, 4 after `Kill`. A connection plus 200 reliable messages made 1,880
    allocations the monitor counted from both threads, and the leak report at process exit showed 0
    leaked blocks. The monitor's numbers include GNS's thread from now on.
  - **A finding for decisions k and m.** The side that closes its own connection still gets one
    event, Connected to None. The prototype notes said it gets none. Systems ignore events whose
    new state is None.
  - **The build.** x64 Debug and Release, 0 warnings, 0 errors. `check-includes.ps1`: 187 files,
    exit 0.
  - **Tests.** `scripts\run-tests.bat` passes in all four configurations.
  - **The game.** `scripts\run-game.ps1 -Scene 0`, F2 to F5: exit 0, 0 `[WARNING]`, 0 `[ERROR]`, 0
    asserts, 0 leaked blocks.
  - **The server.** `-Server -Scene 4 -Ticks 300`: exit 0, no window, "Ran for 300 ticks", 0 leaked
    blocks.
- **Decision h, 2026-11-25.** `net/ReceivedMessage.h` holds a connection number and a copy of the
  bytes. `NET::ServerSocket(port)` opens a listen socket on every local address and a poll group, and
  throws `std::runtime_error` when the listen socket fails; its destructor closes the listen socket,
  then the poll group. `Accept(connection)` calls `AcceptConnection` and `SetConnectionPollGroup`.
  `NET::ClientConnection(address)` calls `ConnectByIPAddress`; its destructor calls `CloseConnection`.
  Both `Receive(out)` calls read in batches of 64, copy each message, `Release` it at once, and stop
  when a batch comes back short. Neither header includes a GNS header, and neither class can be
  copied or moved. No scene uses them until decisions k and m.
  - **Bugs the first read found.** A stray `}` at the end of `ServerSocket.h` stopped the build.
    `ClientConnection::Receive` had no `Release`, and its count and return sat inside the `for`, so
    a call with nothing waiting would loop forever.
  - **A finding: GNS accepts an address with no port.** `ParseString("127.0.0.1")` returns true with
    port 0, and `ConnectByIPAddress` then returns a real handle. The constructor also throws
    `std::invalid_argument` when the port is 0.
  - **A finding for decision i: a busy port does not throw.** Two processes each listened on port
    27095 with no error. `Get-NetUDPEndpoint` showed the first on `::` and the second on `0.0.0.0`:
    Windows lets an IPv4-only socket share a port with a dual-stack one. A second server on the same
    port starts silently.
  - **The receive test.** A scratch program compiles `NetworkLibrary.cpp`, `ServerSocket.cpp`,
    `ClientConnection.cpp` and `MemoryMonitor.cpp`, with a server on port 27096 and a client over
    loopback in one process. 13 of 13 checks: `Accept` returns true and both sides reach Connected;
    `Receive` with nothing waiting returns 0; three messages arrive in order with the right bytes and
    the server-side connection number; 200 messages, past three batches of 64, arrive in order; the
    server's reply reaches the client; an address with no port throws; a second `ServerSocket` on
    the same port opens. The leak report at process exit shows 0 leaked blocks. Decision o moves it
    into `NetTests`.
  - **The build.** x64 Debug and Release, 0 warnings, 0 errors. `check-includes.ps1`: 192 files,
    exit 0.
  - **Tests.** `scripts\run-tests.bat` passes in all four configurations.
  - **The game.** `scripts\run-game.ps1 -Scene 0`, F2 to Scene 1: exit 0, 0 `[WARNING]`, 0 `[ERROR]`,
    0 asserts, 0 leaked blocks.
  - **The server.** `-Server -Scene 0 -Ticks 120`: exit 0, no window, "Ran for 120 ticks", 0 leaked
    blocks.
- **Decision i, 2026-11-25.** `NET::kDefaultPort` is 27020, in `net/Ports.h`. `core/NetworkSettings.h`
  holds `NetworkRole` (Offline, Server, Client) and `NetworkSettings { role, host, port }`, with no
  namespace, as other `core` types. `app/NetworkArguments` reads `--server`, `--port P` and
  `--connect host[:P]`: the port follows the last colon, a missing port is the default, and a port
  must be 1 to 65535. `--server` with `--connect`, and `--port` without `--server`, throw.
  `SceneManager` takes the settings in its constructor and `Set`s them on each new World before
  `OnCreate`. Both applications build the settings before `sceneManager` and print one line with
  the role. `Application` holds `std::optional<NET::NetworkLibrary>` as its first member and fills it
  only for a client, so offline runs no longer start GNS; `Run` polls only when it is filled.
  `ServerApplication` keeps a plain `NetworkLibrary`.
  - **The flags.** Debug exe, run directly: `--server --ticks 60` printed "server on port 27020",
    `--server --port 27030` printed 27030, `--connect 127.0.0.1 --scene 4` printed "client for
    127.0.0.1:27020", and `--connect 127.0.0.1:27031` printed 27031. `--port 5`, `--server --connect
    x`, ports `0`, `70000` and `abc`, `--connect :27020`, a bare `--connect` and a bare `--port` each
    printed their own `[ERROR]` line and exited 1. Every run showed 0 leaked blocks.
  - **GNS off when offline: measured, inside noise.** Boss fight windows after 4 s: every client run
    had 70 threads; offline runs had 69, 69 and 70. The GPU driver's threads move the count by about
    1. .NET's thread start addresses all point into Windows' loader, so they could not single out
    GNS's thread.
  - **The build.** x64 Debug and Release, 0 warnings, 0 errors. `check-includes.ps1`: 196 files,
    exit 0.
  - **Tests.** `scripts\run-tests.bat` passes in all four configurations.
  - **The game.** `--scene 4` offline and as a client both ran `BossFightScene::OnCreate` and closed
    with 0 `[WARNING]`, 0 `[ERROR]` and 0 leaked blocks. A `run-game.ps1 -Scene 0` run exited 0 with 0
    leaked blocks, but its F5 never reached the window: another window held focus.
  - **The server.** `-Server -Scene 4 -Ticks 300`: exit 0, no window, "server on port 27020", "Ran
    for 300 ticks", 0 leaked blocks.
- **Decision j, 2026-11-25.** `net/ByteWriter` appends `u8`, `u16` and `u32` high byte first, and
  hands the bytes back with `GetBytes` or `TakeBytes`. `net/ByteReader` reads them back from a span.
  A read past the end returns 0 and marks the reader invalid, and every later read fails too.
  `net/Messages.h` numbers `MessageType` from 1 (Hello, Welcome, PlayerJoined, PlayerLeft), so a
  zeroed byte is never a type, and sets `kProtocolVersion` to 1. Each message has a `Write` overload
  and a `Read...` function that returns no value for a wrong type, a short message or leftover
  bytes. `PeekMessageType` returns the first byte's type, or no value. `ServerSocket::Send(connection,
  bytes)` and `ClientConnection::Send(bytes)` send reliably and return false when GNS refuses. None
  of these files includes a GNS header except the two `.cpp` files that send.
  - **A bug the test found.** `ReadU32` with three bytes left returned `0x01020000`, not 0: the
    first `ReadU16` passed and only the second failed. Messages were still rejected, because the
    reader was invalid. `ReadU32` now checks for four bytes first.
  - **A finding: Send before Connected succeeds.** A Hello sent while the client was still
    connecting reached the server once the connection came up; GNS holds it until then. `Send`
    returns false for a bad or closed connection, a message over GNS's size limit, or a full send
    queue (`isteamnetworkingsockets.h`, line 259).
  - **The message test.** A scratch program with the three standard-only `.cpp` files, built for
    x64 and Win32 with `/W4`: 28 of 28 checks on both. The writer puts the high byte first; numbers,
    0 and the maximums round-trip; a failed read sticks. Welcome `{0x01020304, 5}` is `02 01 02 03
    04 00 00 00 05`, and Hello, PlayerJoined and PlayerLeft match their bytes. Every cut-short
    Welcome, one with an extra byte and one read as the wrong type give no value. `PeekMessageType`
    rejects an empty span and types 0, 5 and 255, and accepts 1 to 4. Decision o moves it into a
    test target.
  - **The loopback test.** Server on port 27097 and a client in one process: 8 of 8 checks. The
    server reads the client's Hello from the right connection, the client reads Welcome `{7, 99}`,
    and `Send` to connection 0 or to an unknown connection returns false. 0 leaked blocks at exit.
  - **The build.** x64 Debug and Release, 0 warnings, 0 errors. `check-includes.ps1`: 202 files,
    exit 0.
  - **Tests.** `scripts\run-tests.bat` passes in all four configurations.
  - **The server.** `-Server -Scene 4 -Ticks 300`: exit 0, no window, "Ran for 300 ticks", 0
    leaked blocks. The game was not run: nothing in it calls this code yet.
- **Decisions k, l, m and n, 2026-11-25.** Built together, because none can run alone.
  `BossFightScene::OnCreate` reads `NetworkSettings`. A server World inserts a `ServerSocket`, a
  `ClientTable` and a `ConnectionEventQueue`, and runs `AcceptClientsSystem` and then
  `ServerReceiveSystem` in Fixed. A client World inserts a `ClientConnection` and a
  `ConnectionEventQueue`, and runs `ClientReceiveSystem` in Update. Offline adds nothing.
  - **Three changes to the plan.** The server spawns the player on a Hello with the right version,
    not on Connected, so the version check comes first (k). A slot holds the player's NetworkId, not
    its Entity: `CommandBuffer::CreateEntity` returns a placeholder that only lasts its pass, so
    `AcceptClientsSystem` finds the player to despawn by querying `NET::NetworkIdComponent` (l). The
    slots are a `std::array` of 8, because the cap is fixed (l).
  - **What each part does.** `ClientTable` has 8 `ClientSlot { connection, clientId,
    playerNetworkId }`, two counters from 1, `Find(connection)` (`Find(0)` gives a free slot) and
    `PlayerCount()`. `ServerSocket::Close(connection, reason)` calls `CloseConnection`. On
    Connecting, `AcceptClientsSystem` accepts into a free slot, or closes with "server full". On
    ClosedByPeer or ProblemDetectedLocally, it closes the handle (GNS keeps it until then,
    `steamnetworkingtypes.h` lines 403 to 416), queues the player's despawn, sends `PlayerLeft` to
    the others and frees the slot. `ServerReceiveSystem` answers a Hello with a player, `Welcome`
    and `PlayerJoined` to the others; a wrong version closes the connection and frees the slot.
    `ClientReceiveSystem` prints "connecting", sends Hello on Connected, prints Welcome,
    PlayerJoined and PlayerLeft, and throws when the server connection ends (m).
  - **A bug the build found.** `ClientReceiveSystem`'s constructor did not call `System`'s, which
    has no default: C2512.
  - **The join test.** A scratch script started the Debug server with `--ticks 2400`, client A at
    2 s and client B at 9 s, closed A at 16 s and B at 19 s, then started client C at 22 s. The
    server printed players 1, 2, 1, 0, then 1 for C. A printed "player 2 joined", and B printed
    "player 1 left". A, B and the server exited 0. Every process showed 0 leaked blocks, no
    `[WARNING]` or `[ERROR]` before shutdown, and 0 world allocations that fell to the heap.
  - **A gap the test found: the server leaves without a word.** The server exited at 40.3 s. C
    printed `[ERROR] lost the server: Connection dropped` and exited 1 at 53.1 s, 12.8 s later,
    after a timeout. The likely cause, not yet measured: the process ends before GNS's thread
    sends the close. The next decision closes each client with a reason before GNS stops.
  - **Not tested.** The server-full path and the wrong-version path. A late joiner does not learn
    about the players already there; step 5's snapshots cover that.
  - **The build.** x64 Debug and Release, 0 warnings, 0 errors. `check-includes.ps1`: 211 files,
    exit 0.
  - **Tests.** `scripts\run-tests.bat` passes in all four configurations.
- **Decision q, 2026-11-25: the server closes its clients before it stops.** Added after the join
  test. GNS's header names the cause of the 12.8 s wait: `CloseListenSocket` closes every accepted
  connection "ungracefully" (`isteamnetworkingsockets.h`, line 193), so no close reaches the client.
  `ServerApplication::CloseClients` runs after the loop in `Run`. It calls `ServerSocket::Close` on
  every slot in the `ClientTable` with the reason "server closed", frees each slot, then polls GNS
  for `kCloseWait`, 200 ms, so GNS's thread can send the closes before the program ends.
  `ServerSocket::Close` now passes linger as true, so GNS tries to send queued reliable messages
  before it closes (line 184). The close lives in `Run`, not in `~ServerSocket`, because the socket
  does not know its connections; the `ClientTable` does.
  - **Measured.** In `check-join.ps1`, client C wrote `[ERROR] lost the server: server closed` at
    23:31:05.673, and the server finished its log at 05.744: C heard the close inside the 200 ms
    wait. Before q it took 12.8 s. Loopback only: a real network adds its trip time.
- **Decision o, 2026-11-25: two test programs.**
  - **`MessageTests`**, built for x64 and Win32, compiles only `ByteWriter`, `ByteReader` and
    `Messages`. 29 checks: the high byte first, round trips of every width with 0 and the
    maximums, a read past the end returning 0 and staying failed, the exact bytes of all four
    messages, the four round trips, every cut-short Welcome, an extra byte, the wrong type, and
    `PeekMessageType` on an empty span and types 0, 5, 255 and 1 to 4.
  - **`NetTests`**, x64 only, holds a server World and two client Worlds in one program over
    loopback on port 27098, each built as `BossFightScene` builds it. Every World gets every
    event, as the applications hand them over, and each World's prints go to its own log. 13
    checks: A is welcomed as client 1 with NetworkId 1; B as client 2; A hears that player 2
    joined and B hears nothing about itself; each client sent one Hello though every World saw
    every event; the server holds 1, 2, 1, then 0 players, counted in both the `ClientTable` and
    the entities with a `NetworkIdComponent`; B hears that player 1 left; the server logs both
    leaves. It sits after the ECS targets in `CMakeLists.txt`, not in the engine's x64 branch,
    because it needs `ECS_SOURCES`.
  - **Wiring.** Both have an `add_test` line, NetTests only where its target exists, and
    `scripts\run-tests.bat` builds them.
- **Decision p, 2026-11-25: `scripts/check-join.ps1`.** Starts the Debug server with `--ticks
  1800`, client A at 2 s, client B at 9 s, closes A at 16 s and B at 19 s, starts client C at
  21 s, and lets the server stop on its own. It reads each process's log and passes when the
  server prints players 1, 2, 1, 0, 1, A hears that player 2 joined, B that player 1 left, C
  prints "lost the server: server closed" within 2 s of the server's exit and exits 1, the server,
  A and B exit 0, and all four show 0 leaked blocks and no `[WARNING]`.
  - **The first run.** 15 of 15 checks passed, with C's exit 0.0 s after the server's.
- **What the build checked, for q, o and p.** x64 Debug and Release, 0 warnings, 0 errors.
  `check-includes.ps1`: 211 files, exit 0. `scripts\run-tests.bat` passes in all four
  configurations: 7 of 7 programs on x64 (NetTests 13 of 13, MessageTests 29 of 29) and 5 of 5 on
  Win32. `check-join.ps1` as above.

## 4. Input as unreliable messages

## 5. Snapshots as unreliable messages

## 6. Interpolation

## 7. Shooting

## 8. The lag test
