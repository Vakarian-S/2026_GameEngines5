# Plan

**Last updated:** 2026-11-20.

This plan builds the Boss Fight Scene: a headless server runs a small boss fight, and clients connect
to it through Valve's GameNetworkingSockets (GNS). Joins and leaves go as reliable messages, input and
snapshots as unreliable ones, and clients draw the snapshots with interpolation. The server decides
everything, and there is no prediction. The goal is the engine's first networking: a headless mode
and a server and client split that later networked features and headless tests reuse. The course's
networking topic comes along with it. It came from ROADMAP.md, Networking, Boss Fight Scene row 1.

## Steps

| # | step | done when | stage |
|---|---|---|---|
| 1 | A headless mode: the engine starts with a server flag, opens no window, and the Fixed phase still ticks | A server run prints its tick count, opens no window, and closes clean | designed |
| 2 | The Boss Fight Scene offline: a boss on a pattern, a player that moves and shoots, in one process | The scene plays in one window, with no network | designed |
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
  the first chunk of a `Query` on `Transform` and `FixedStepRow`.
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
  | `-Server -Scene 0 -Ticks 60` | exit 0, no tick lines (Scene0 has no `FixedStepRow`), 0 leaked blocks |
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
- **c. Input.** A `PlayerCommand` component on each player entity holds a move direction (WASD) and
  an aim direction (the arrow keys), each normalized. `PlayerCommandSystem`, in Update, reads
  `InputState` and rewrites the local player's whole command every frame. It is the only writer of
  `PlayerCommand`, and Fixed systems read only the command, never `InputState`. A held arrow aims and
  fires at once, as in a twin-stick shooter; there is no fire flag, so a shot can never have a zero
  direction. The gun's cooldown sets the rate, and also stops a second shot when one frame runs two
  fixed steps. Changed 2026-11-24: Space fired along the last move direction, and moving, aiming and
  firing needed three hands. In step 4 the server fills the same
  component from the network, and no Fixed system changes. A component, not a resource, because the
  server holds one command per player. Overwatch does the same: its input system fills a singleton
  input from the OS, and its command system "becomes the only place" that changes the `PlayerCommand`
  that is networked and simulated (GDC 2017, "Overwatch Gameplay Architecture and Netcode", 13:52 to
  14:56). Rejected: Fixed systems that read `InputState`, and a fire latch that a Fixed system clears,
  because it adds a second writer.
- **d. Movement.** A player system turns the command into a `PHYSICS::Velocity`, the existing
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
  gets `Transform`, `Velocity` along the aim, `LifespanComponent { ticksLeft }` and a `Renderable`.
  The player's `GunComponent` in `game/Components.h` holds `readyTick` and the stats: projectile
  speed, lifespan ticks, cooldown ticks and the projectile's `Renderable`. Stats on the component let
  a second weapon be a different `GunComponent{}`, with no new system; one component passes ECS.md
  rule 3, because only `ShootSystem` reads it. `LifespanComponent` lives in `core/Components.h`, and
  `LifespanSystem` in `core/systems` counts it down in Fixed and destroys the entity at 0 through the
  command buffer, so a hit spark or a pickup can reuse it. A projectile also despawns on a hit.
  Rejected: aiming with the mouse, which needs a ray to the floor; stats as `ShootSystem` constants,
  which allow one weapon only; a shared weapon table, which pays off only when many entities share
  a weapon; splitting `GunComponent` into state and stats, which pays off only with many guns; and a
  `Projectile` component in `game`.
- **h. Hits.** The boss and each projectile get a `SphereCollider`. The test lives in
  `include/physics/Collision3D.h` and `src/physics/Collision3D.cpp`, free functions in `PHYSICS` next to
  `RayIntersect.h`. This step adds `Overlap(Transform, SphereCollider, Transform, SphereCollider)`,
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
  `BossDeathSystem` on `BossComponent + HealthComponent` does both, in Fixed after
  `ProjectileHitSystem`, so hits only change health (rule 8) and death is a scan, not a callback
  (ECS.md rule 7). A general death system waits until something other than the boss has health. R
  restarts the fight with `World::Clear`, as `PhysicsScene` does. The scene loads its assets once in
  `OnCreate` and keeps three `Renderable` members, so `SpawnEntities` needs no asset loader. It is
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
  `game/FigureEightPath.h` gives the offset. A `FigureEightPath` component holds each entity's
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
- **Decision g, 2026-11-24.** `PlayerCommand` holds `moveDirection` and `aimDirection`, with no fire
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
  between two centres with the sum of `radius * scale.x`. Projectiles carry `SphereCollider` and
  `DamageComponent`; the boss carries `SphereCollider` (radius 0.5, so 1.5 m at scale 3) and
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
  the player and the boss from the stored `Renderable` members, and `Update` calls `world.Clear()`
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
- **Decision k, 2026-11-24.** A floor entity with `Transform` and `Renderable` only: the cube mesh,
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
- **b. Where vcpkg comes from.** `VCPKG_ROOT` points at an installed vcpkg, as `VULKAN_SDK` points at
  the Vulkan SDK, and only the x64 presets set its toolchain file. The engine already needs one
  installed SDK. A git submodule was rejected: it adds 181 MB to every clone.
- **c. The two extra DLLs.** The build tries the `x64-windows-static-md` triplet, so protobuf and
  Abseil link into the exe. If that fails, it copies `libprotobuf.dll` and `abseil_dll.dll` next to
  the exe. Untested; the first build decides.
- **d. The `net` module.** `include/net` and `src/net`, with the standard library and the GNS headers
  only. Those headers pull in no `windows.h` and no protobuf. `game` and `app` may include `net`;
  `core`, `physics` and `ecs` never do. Rule 2 gains that line.
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

## 4. Input as unreliable messages

## 5. Snapshots as unreliable messages

## 6. Interpolation

## 7. Shooting

## 8. The lag test
