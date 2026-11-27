# Plan

**Last updated:** 2026-11-26.

This initiative builds the Boss Fight Scene: a headless server runs a small boss fight, and clients
connect to it through Valve's GameNetworkingSockets (GNS). Joins and leaves go as reliable messages,
input and snapshots as unreliable ones, and clients draw the snapshots with interpolation. The server
decides everything, and there is no prediction. The goal is the engine's first networking: a headless
mode and a server and client split that later networked features and headless tests reuse.

## Epics

| # | epic | done when | stage |
|---|---|---|---|
| E1 | A headless mode: the engine starts with a server flag, opens no window, and the Fixed phase still ticks | A server run prints its tick count, opens no window, and closes clean | done (d58e448) |
| E2 | The Boss Fight Scene offline: a boss on a pattern, a player that moves and shoots, in one process | The scene plays in one window, with no network | done (cacd49e, 1d06049) |
| E3 | Join and leave over GNS: the server hands the client an ID, and spawns or despawns its player | Two clients join and leave, and the server's player count follows | done (b8b2036, 51c2ac0) |
| E4 | Input as unreliable messages: WASD on a client moves its player on the server | The server logs each player moving from its own client's keys | outline |
| E5 | Snapshots as unreliable messages, mirrored on the client through `NetworkIdMap` and drawn raw | Both clients show the boss and both players, with visible jitter | outline |
| E6 | Interpolation from a `SnapshotBuffer`, drawn about 100 ms in the past | The jitter from E5 is gone | outline |
| E7 | Shooting: fire goes up as input, and the projectile comes back in a snapshot | A client fires, and both clients see the projectile | outline |
| E8 | A lag test: a stats line (ping, loss, snapshot age), a switch that sends snapshots as reliable messages, and GNS's fake loss at 5% | Under 5% loss, unreliable snapshots skip a frame and reliable ones freeze then jump | outline |

| stage | meaning |
|---|---|
| outline | the idea and its reason, from ROADMAP.md, not designed yet |
| designed | the tasks and decisions are written, ready to build |
| done | built, recapped and committed, with the commit hash |

---

## E1. Headless mode

**The problem.** `Application` builds its `Window` and `VulkanRenderer` in its constructor, and each
scene's `OnCreate` loads meshes through the renderer's `AssetLoader`. A server has no window and no
renderer. Agreed 2026-11-20.

| task | title |
|---|---|
| E1.1 | One exe, started with `--server` |
| E1.2 | A `NullAssetLoader` on the server |
| E1.3 | The server runs only the Fixed phase |
| E1.4 | `FrameCap` paces the server at 60 ticks a second |
| E1.5 | Ctrl+Q stops the server |
| E1.6 | `--ticks N` stops the server after N ticks |
| E1.7 | `--scene N` picks the scene |
| E1.8 | SDL's clock with no window |
| E1.9 | The server stays x64 only |
| E1.10 | A server check in `run-game.ps1` |

### E1.1. One exe, started with `--server`

`main` reads the flag and builds either `Application` or a new `ServerApplication`, both in `app`,
then calls `Run`. `main` stays the only file that includes `app`, and the build stays one target.

- **Chose one exe over a separate server exe,** because a second exe needs the sources split into
  libraries, and it only proves at link time that the server never links Vulkan. Unreal builds a
  separate exe from `<Project>Server.Target.cs`
  ([Unreal docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine));
  Unity offers both, `-batchmode -nographics` and a Server build subtarget
  ([command line](https://docs.unity.com/en-us/engine/current/manual/unity-editor/command-line-arguments/player),
  [dedicated server](https://docs.unity.com/en-us/engine/current/manual/platform-specific/dedicated-server/build)).
- **Chose a second application class over an optional window and renderer inside `Application`,**
  because every line of the loop would branch on them.

### E1.2. A `NullAssetLoader` on the server

A `NullAssetLoader` in `core` returns empty handles. `ServerApplication` hands it to its
`SceneManager`, and `Application` keeps handing over the renderer's `AssetStore`.

- **Chose a loader picked by the application over a server-only path in each scene's `OnCreate`,**
  because every `OnCreate` then runs unchanged, and nothing checks the role at runtime. Handles mean
  nothing on the server, because snapshots send a kind byte.

### E1.3. The server runs only the Fixed phase

One tick is one fixed step. The client still runs Update and Extract each frame, for its camera,
input, interpolation and drawing. Extract would assert on the server anyway, because nothing sets
`DrawListTarget`.

- **Chose Fixed only over Update at a free frame rate,** because everything the server decides lives
  in Fixed. Unity's Netcode for Entities splits it the same way: the server runs a fixed time step,
  60 ticks a second by default, and presentation systems exist only in the client world
  ([Unity docs](https://docs.unity3d.com/Packages/com.unity.netcode@1.3/manual/client-server-worlds.html)).

### E1.4. `FrameCap` paces the server at 60 ticks a second

`ServerApplication` uses `FrameCap` to sleep until the next tick. With no FIFO present to wait on, a
bare loop would spin a core (a guess, not measured).

### E1.5. Ctrl+Q stops the server

Ctrl+Q ends the loop, so the memory report and the leak check still print. `ServerApplication` reads
the console's input buffer once per tick: `GetNumberOfConsoleInputEvents` gives the count of unread
records, and `ReadConsoleInput` runs only when the count is above 0, because it waits for a record
([docs](https://learn.microsoft.com/en-us/windows/console/readconsoleinput)). The code lives in
`ServerApplication`, so `core` never includes `windows.h`.

- **Chose to keep Ctrl+C as Windows' hard kill over a Ctrl+C handler that sets a flag,** because the
  handler runs on a new thread ([docs](https://learn.microsoft.com/en-us/windows/console/handlerroutine)),
  and the hard kill still works when a bug hangs the loop.

### E1.6. `--ticks N` stops the server after N ticks

For scripted checks, which have no console to type Ctrl+Q in. Without the flag, the server runs until
Ctrl+Q. Removing the flag from a shipping build belongs to ROADMAP.md, Debug tools, A shipping build.

- **Chose a tick limit over killing the process,** because a kill skips the reports.

### E1.7. `--scene N` picks the scene

Each flag does one job: `--server` makes the run headless, `--ticks N` sets the lifespan, and
`--scene N` picks the row. The game takes `--scene N` too, in place of a bare `argv[1]`, so one parser
serves both applications and flags come in any order. No `--scene` picks the last row. E1 runs
`--server --scene 3`, `PacingScene`, whose bottom row moves in Fixed.

- **Vakarian chose a flag over a bare number after `--server`,** on 2026-11-20, because a bare
  number gives one flag two jobs and reads less clearly.

### E1.8. SDL's clock with no window

`Timer` and `FrameCap` call `SDL_GetTicksNS`, and only `Window` calls `SDL_Init`. SDL's docs do not say
whether the clock needs it ([SDL_GetTicksNS](https://wiki.libsdl.org/SDL3/SDL_GetTicksNS)). Settled
2026-11-20: the clock works without `SDL_Init`, so the server does not call it.

### E1.9. The server stays x64 only

SDL3 and the engine sources build only on x64 (`CMakeLists.txt`, the `GameEngines5` branch), and
`Timer` needs SDL.

### E1.10. A server check in `run-game.ps1`

`scripts\run-game.ps1` gets a `-Server` switch that skips its window steps, and `-Scene N` now passes
`--scene N`. The check runs `--server --scene 3 --ticks 600` and passes with exit 0, no window,
"tick 600", no `[WARNING]` or `[ERROR]`, and 0 leaked blocks.

### What was built

Built 2026-11-20.

- `ServerApplication` in `include/app` and `src/app`. Every 60 ticks it prints `tick N, mario x X`.
- `Main.cpp`: `HasServerFlag` picks the application, and both fall through to the profiler, memory
  and leak reports.
- `include/core/NullAssetLoader.h`, header only.
- `include/app/SceneIndex.h` and `src/app/SceneIndex.cpp`: the scene table and `PickScene`, moved out
  of `Application.cpp`.
- `GetTickLifespan` in `ServerApplication.cpp` reads `--ticks N`.
- `windows.h` comes after every engine header in `ServerApplication.cpp`, because it defines `near`
  and `far` as empty macros, which break `MMath.h`.
- `.run/GameEngines5 Server.run.xml` runs the server in an external console, because Rider's run
  window pipes stdin and Ctrl+Q never arrives.
- `scripts\run-game.ps1`: `-Scene N`, and `-Server` with `-Ticks N` (600 by default).

### What was checked

x64 Debug. The build has no errors and no warnings. No tests were added: E1 has no math or ECS code
to test on its own. `scripts\run-tests.bat` passes in all four configurations.

| run | result |
|---|---|
| `run-game.ps1 -Server -Scene 3 -Ticks 600` | exit 0, no window, 10 tick lines from tick 60 to 600, "Ran for 600 ticks", 0 `[WARNING]`, 0 `[ERROR]`, 0 leaked blocks |
| `run-game.ps1 -Server -Scene 0 -Ticks 60` | exit 0, no tick lines (Scene0 has no `FixedStepRowTag`), 0 leaked blocks |
| `run-game.ps1 -Server -Scene 9 -Ticks 60` | exit 1, prints the scene list |
| `--ticks 120` by hand | ran 2.0 s |
| `--ticks abc`, `--ticks 0`, a bare `--ticks` | each printed its message and exited 1 |
| no `--ticks` | still running after 15 s |
| Ctrl+Q, by Vakarian in a terminal | stopped the server |
| `run-game.ps1 -Scene 2`, F4, then F1 | exit 0, three scenes closed, 0 `[WARNING]`, 0 `[ERROR]`, 0 leaked blocks |

- **Real-time speed.** The Mario x moves 10 units a second and wraps from 15 to -15, which matches
  `kSpeed` 10 and `kPathWidth` 30.
- **Pacing.** 600 ticks took 10.02 s of wall time and 0.09 s of CPU: a `Stopwatch` around
  `GameEngines5.exe --server 3`, x64 Debug, and the process's `TotalProcessorTime`.

## E2. The Boss Fight Scene offline

**The problem.** A fight that plays in one window, before any network exists. Its systems split into
server work and client work now, so E3 only picks which lists run. Agreed 2026-11-20.

| task | title |
|---|---|
| E2.1 | A fifth scene, on F5 |
| E2.2 | A fixed overhead camera |
| E2.3 | `PlayerCommandComponent` holds the player's input |
| E2.4 | Movement through the existing integrator |
| E2.5 | A `FixedTickCount` resource |
| E2.6 | The boss follows a figure-eight |
| E2.7 | Shooting |
| E2.8 | Hits |
| E2.9 | The boss dies, and R restarts |
| E2.10 | Server and client system lists |
| E2.11 | Assets and the floor |
| E2.12 | The final check |

### E2.1. A fifth scene, on F5

The app's F1 to F4 range grows to F5. The other scenes stay, because they are benchmarks.

### E2.2. A fixed overhead camera

Angled down, with play on the XZ plane.

- **Chose a fixed camera over the fly camera,** because WASD moves the player, and the fly camera's
  WASD would fight it.

### E2.3. `PlayerCommandComponent` holds the player's input

It holds a move direction (WASD) and an aim direction (the arrow keys), each normalized.
`PlayerCommandSystem`, in Update, reads `InputState` and rewrites the local player's whole command
every frame. It is the only writer, and Fixed systems read only the command. A held arrow aims and
fires at once, as in a twin-stick shooter. The gun's cooldown sets the fire rate, and also stops a
second shot when one frame runs two fixed steps. In E4, the server fills the same component from the
network, and no Fixed system changes.

- **Chose a component over a resource,** because the server holds one command per player. Overwatch's
  command system is the only place that changes its networked `PlayerCommandComponent` (GDC 2017,
  "Overwatch Gameplay Architecture and Netcode", 13:52 to 14:56).
- **Chose Fixed systems that read the command over Fixed systems that read `InputState`,** so in E4
  the server fills the same component from the network, and no Fixed system changes.
- **Chose no fire flag over a fire latch that a Fixed system clears,** because the latch adds a second
  writer. Without a flag, a shot can never have a zero direction.
- **Changed 2026-11-24: the arrows fire, not Space.** Space fired along the last move direction, so
  moving, aiming and firing needed three hands.

### E2.4. Movement through the existing integrator

`PlayerMoveSystem` turns the command into a `PHYSICS::VelocityComponent`, the existing
`UpdatePositionSystem` moves the player, and `ClampToArenaSystem` keeps it inside the arena.

### E2.5. A `FixedTickCount` resource

It counts fixed steps. `Application` and `ServerApplication` add 1 and `Set` it before each Fixed
run. The boss path, E1's printout and E5's snapshot header read it. It is ROADMAP.md, Fixed step
additions row 3.

### E2.6. The boss follows a figure-eight

Its position is a free function of the tick, so it holds no state, and a test checks it with no ECS
(rule 9). The boss does not attack in this epic: nothing in the outline needs it.

- **Chose a function of the tick over a path stepped by delta time,** because stepping drifts.

### E2.7. Shooting

`ShootSystem`, in Fixed, spawns a projectile through the command buffer while the aim is not zero and
the tick has reached the gun's `readyTick`. The projectile gets `TransformComponent`,
`VelocityComponent` along the aim, `LifespanComponent { ticksLeft }` and a `RenderableComponent`.
`GunComponent` holds `readyTick` and the stats: projectile speed, lifespan ticks, cooldown ticks and
the projectile's `RenderableComponent`. `LifespanComponent` lives in `core/Components.h`, and
`LifespanSystem` in `core/systems` destroys the entity at 0, so a hit spark or a pickup can reuse it.

- **Chose stats on `GunComponent` over constants in `ShootSystem`,** because constants allow one
  weapon only. A second weapon is a different `GunComponent{}`, with no new system.
- **Chose one component over a shared weapon table, or a state and stats split,** because those pay
  off only when many entities share a weapon, or with many guns.
- **Chose the arrow keys over aiming with the mouse,** because the mouse needs a ray to the floor.

### E2.8. Hits

The boss and each projectile get a `SphereColliderComponent`. `PHYSICS::Overlap` for two spheres lives
in `include/physics/Collision3D.h`, next to `RayIntersect.h`, so both `physics` and `game` reach it.
Collision row 1 later adds the other shape pairs and a `Collide(...)` that returns a contact.
`ProjectileHitSystem`, in Fixed, pairs entities with `DamageComponent` against entities with
`HealthComponent`: the boss's health drops by the damage, and the projectile despawns. Projectiles
are the outer loop, and a projectile stops at its first hit, so it deals damage once even though its
destroy waits for the end of the phase.

- **Chose a `DamageComponent` over a `Projectile` tag** to mark what can hit, because the amount comes
  from the gun's stats.
- **Chose `int32` health and damage over unsigned,** because health is subtracted and can pass below
  0. Unsigned health wrapped from 0 to 4294967295 on the first hit.
- **`Collision2D.h` waits for its first caller,** because nothing has a 2D collider.

### E2.9. The boss dies, and R restarts

At 0 health, `BossDeathSystem` on `BossTag` and `HealthComponent` despawns the boss and prints "boss
died". It runs in Fixed after `ProjectileHitSystem`, so hits only change health. R restarts the fight
with `World::Clear`, as `PhysicsScene` does. The scene loads its assets once in `OnCreate` and keeps
three `RenderableComponent` members, so `SpawnEntities` needs no asset loader. The death is the rare
event a reliable message carries later.

- **Chose a scan over a callback,** per ECS.md rule 7. A general death system waits until something
  other than the boss has health.

### E2.10. Server and client system lists

Path, movement, shooting, lifespan and hits go in Fixed, the server's list. `PlayerCommandSystem` goes
in Update and drawing in Extract, the client's list. E3 then picks lists by role with no system
rewritten.

### E2.11. Assets and the floor

Mario for the player, the Triceratops with its palette for the boss, a small cube for projectiles,
and a flat scaled cube for the floor, all existing models. The floor is spawned in `SpawnEntities`,
so it survives R, with `floor_grey.png` and no collider.

### E2.12. The final check

Tests: the boss path at known ticks in `PhysicsTests`, `LifespanSystem` in `EcsTests`, and the sphere
`Overlap` in `PhysicsTests`. `scripts\run-game.ps1 -Scene 4` holds WASD and the arrow keys. The
screenshots show the player moving and projectiles in flight, the console shows "boss died", and the
run ends with 0 leaked blocks.

### What was built

Built 2026-11-21 to 2026-11-24.

- **E2.6.** `FigureEightPathPosition` in `game/FigureEightPath.h`. A `FigureEightPathComponent` holds
  each entity's center, period and sizes, so several entities can loop in different spots.
  `FigureEightPathSystem` was first named `BossPatternSystem`.
- **E2.7.** `ShootSystem`, `GunComponent`, `LifespanComponent` and `LifespanSystem`, in Fixed after
  `ClampToArenaSystem`. `EcsTests` now links `MathLibrary`, because `core/Components.h` needs the math
  headers. `run-game.ps1` sends the arrow keys with the extended-key flag, because their codes carry
  0xE0.
- **E2.8.** `physics/Collision3D` and `ProjectileHitSystem`, in Fixed after `LifespanSystem`.
- **E2.9.** `BossDeathSystem`. `BossFightScene::SpawnEntities` creates the player and the boss, and
  `Update` calls `world.Clear()` then `SpawnEntities()` on R.
- **E2.10.** Nothing to build: `BossFightScene::OnCreate` already split the lists. R in
  `BossFightScene::Update` is the one client-only piece outside them.
- **E2.11.** `kWorldArenaBytes` went from 64 KB to 256 KB. Each archetype takes at least one 16 KB
  chunk, and the floor, player, boss and projectile archetypes filled all 64 KB.

### What was checked

x64 Debug. After every task, `run-game.ps1 -Server -Scene 4 -Ticks 180` exited 0 with no window and
0 leaked blocks, and `scripts\run-tests.bat` passed in all four configurations.

| task | test | checks |
|---|---|---|
| E2.6 | `PhysicsTests`, "a figure-eight path loops through the origin" | 8: ticks 0, half and whole periods give the origin; an eighth, a quarter and three quarters give their points; 1000 periods later gives the same point; y stays 0 |
| E2.7 | `EcsTests`, "LifespanSystem" | 6: each run takes 1 tick, an entity is gone after its last tick, and one with no lifespan stays |
| E2.8 | `PhysicsTests`, "two spheres overlap" | 9: inside and outside the radius, touching does not count, order does not matter, scale 3 reaches 1.9 but not 2.1, every axis counts |
| E2.9 | `EcsTests`, "BossDeathSystem" | 4: bosses at 0 and -3 health go, a boss at 1 stays, a non-boss at 0 stays |

The game, `run-game.ps1 -Scene 4`, every run with exit 0, 0 `[WARNING]`, 0 `[ERROR]` and 0 leaked
blocks:

- **E2.6.** Screenshots 2 s apart show the boss moving clear of Mario. F1, then F5, restarts the boss
  near the start of its path, so the `FixedTickCount` reset works.
- **E2.7.** Up held shows cubes about 3 m apart, which matches 15 m/s at 5 shots a second (cooldown 12
  ticks). 2 s with nothing held removes every cube.
- **E2.8.** Up held for 6 s: health printed 19 down to 10, one point per hit.
- **E2.9.** Up held for 20 s: the boss crosses the line of fire only part of the time, so 20 hits take
  about 18 s. "boss died" printed once, and R brought Mario and the boss back.
- **E2.11.** The floor shows, Mario clamps at its edge, and no world allocation fell to the heap.
- **E2.12, the final run.** WASD moves Mario, Up held for 23.5 s kills the boss, and R brings back the
  floor, Mario and the boss. `EcsTests` 386 checks, `PhysicsTests` 235. The server ran 600 ticks.

## E3. Join and leave over GNS

**The problem.** Every copy of the game runs alone. This epic lets the server know who is connected:
a client starts, finds the server and says hello; the server gives it a number and spawns a player
for it; when the client quits, the server removes that player. Movement and drawing over the network
come in E4 and E5. Agreed 2026-11-20.

**Why GNS.** Decided 2026-11-20, after a prototype. GNS does the
transport work a shipped game needs: whole messages, reliable and unreliable on one connection,
resends, encryption, connection stats, and fake lag and loss. Raw Winsock and SDL_net 3.2.0 were
weighed first. Starting with Winsock and swapping later was rejected, because the TCP and UDP split
would shape E3, E4 and E8, and a swap would throw that code away. The costs, from the prototype: about
7.6 min on a machine's first cold configure, and about 750 MB of installed libraries per platform.

**One fact shapes most tasks.** GNS is one object per program. `GameNetworkingSockets_Init` asserts on
a second call (`csteamnetworkingsockets.cpp`, line 2506, at v1.6.1), and `RunCallbacks()` runs the
callbacks for every connection in the program.

| task | title |
|---|---|
| E3.1 | Build GNS with FetchContent and BCrypt |
| E3.2 | vcpkg from Visual Studio Build Tools 2026 |
| E3.3 | Link protobuf and Abseil statically |
| E3.4 | The `net` module |
| E3.5 | One `NetworkLibrary` per program |
| E3.6 | Connection events reach the right World |
| E3.7 | GNS's own thread stays on |
| E3.8 | Receiving messages |
| E3.9 | The command line and the role |
| E3.10 | Messages and their byte layout |
| E3.11 | Joining and leaving |
| E3.12 | `ClientTable` and `NetworkIdComponent` |
| E3.13 | Connecting from the client |
| E3.14 | The network systems |
| E3.15 | Two test programs |
| E3.16 | `scripts/check-join.ps1` |
| E3.17 | The server closes its clients before it stops |

### E3.1. Build GNS with FetchContent and BCrypt

FetchContent pulls GNS at tag `v1.6.1` and builds its static lib with `USE_CRYPTO=BCrypt`, as the
engine fetches SDL. Our own `vcpkg.json` lists only `protobuf`, so OpenSSL is never built.

- **Chose FetchContent over vcpkg's GNS package,** because the package lists OpenSSL as a hard
  dependency.

### E3.2. vcpkg from Visual Studio Build Tools 2026

vcpkg is the copy bundled with Build Tools 2026 (the "vcpkg package manager" component), and
`VCPKG_ROOT` points at it, as `VULKAN_SDK` points at the Vulkan SDK. Only the x64 preset sets the
toolchain file, and `CMakeLists.txt` stops with "vcpkg was not loaded" when it is missing. Both
presets use the `Visual Studio 18 2026` generator and the `v145` toolset, because vcpkg builds
protobuf with the newest compiler on the machine, and the engine must use the same one. Changed
2026-11-24 by the first build.

- **Chose the bundled vcpkg over a separate clone,** because the clone built protobuf with `v145`
  while the engine used `v143`. Rider also loaded VS's environment, so Rider and the terminal used
  different vcpkg copies.
- **Chose the bundled vcpkg over a git submodule,** because the submodule adds 181 MB to every clone.

### E3.3. Link protobuf and Abseil statically

The x64 preset sets both `VCPKG_TARGET_TRIPLET` and `VCPKG_HOST_TRIPLET` to `x64-windows-static-md`,
so protobuf and Abseil link into the exe and ship no DLL. The host triplet matches, because protobuf
needs itself as a host tool (`protoc`), and a different host triplet builds protobuf twice.
`set(Protobuf_USE_STATIC_LIBS ON)` goes with the GNS options. With it off, GNS adds
`PROTOBUF_USE_DLLS` (its `CMakeLists.txt`, lines 210 to 211), and a program calling GNS fails to
link with 30 unresolved `__declspec(dllimport) google::protobuf::...` symbols.

### E3.4. The `net` module

`include/net` and `src/net` hold the standard library and the GNS headers only; those headers pull in
no `windows.h` and no protobuf. `game`, `scenes` and `app` may include `net`; `core`, `physics` and
`ecs` never do. `scenes` joined the list on 2026-11-25, because a scene inserts the `ClientTable`
resource. `ByteWriter`, `ByteReader`, `Messages` and `net/Components.h` include no GNS header, so
targets without GNS can use them. STATE.md rule 2 has the same lines.

### E3.5. One `NetworkLibrary` per program

`NET::NetworkLibrary` calls `GameNetworkingSockets_Init` in its constructor and
`GameNetworkingSockets_Kill` in its destructor. The application owns it. STATE.md rule 12 has the
written exception: the library lives per program, and every connection, poll group and table lives
per World.

- **Chose the application over a World resource,** because a listen server's second World would call
  `Init` again.

### E3.6. Connection events reach the right World

The application calls `RunCallbacks()` once per frame, before the phases. One callback in `net` copies
each status change into the `NetworkLibrary`'s event list. A GNS callback takes no context pointer, so
it reaches the library through one static pointer. Each World's system takes only the events for its
own listen socket or connection, using `m_hConn` and `m_info.m_hListenSocket`
(`isteamnetworkingsockets.h` line 1024, `steamnetworkingtypes.h` line 688).

- **Chose one callback and a filter over one callback per World,** because GNS gives no way to tell
  the Worlds apart.

### E3.7. GNS's own thread stays on

Its default service thread does the socket work, and our thread only reads results. `MemoryMonitor`
already locks around its counters, so counting from two threads is safe, and its stats include that
thread's allocations.

- **Chose the service thread over manual poll mode,** because the poll functions are in no public
  header.

### E3.8. Receiving messages

The server puts its connections in one poll group per World and reads it with
`ReceiveMessagesOnPollGroup` in Fixed. The client reads its connection with
`ReceiveMessagesOnConnection` in Update.

### E3.9. The command line and the role

`--server --scene 4 [--port P]` and `--connect host[:P] --scene 4`, one flag per job as E1.7 sets, and
one default port in `net`. `NetworkSettings { role, host, port }` lives in `core`, with role Offline,
Server or Client. `SceneManager` takes it in its constructor and `Set`s it on each new World before
`OnCreate`, and the scene picks its system lists from it. No flag means Offline, so E2's fight still
runs.

### E3.10. Messages and their byte layout

The first byte is the type. `ByteWriter` and `ByteReader` write the fields one by one in big-endian
order, and the reader checks bounds. These go reliable: `Hello { protocolVersion }`, `Welcome {
clientId, playerNetworkId }`, `PlayerJoined` and `PlayerLeft { clientId }`. GNS delivers whole
messages, so there is no length prefix.

- **Chose writing fields one by one over a `memcpy` of a struct,** because a struct carries padding
  bytes.

### E3.11. Joining and leaving

On Connecting at the listen socket, the server calls `AcceptConnection`, or `CloseConnection` when all
8 slots are taken. On a Hello with the right version, it spawns the player through the command buffer
and sends `Welcome`. On ClosedByPeer or ProblemDetectedLocally, it despawns the player, closes the
handle and frees the slot.

### E3.12. `ClientTable` and `NetworkIdComponent`

`ClientTable` is a server resource: a `std::array` of 8 slots, each with an `HSteamNetConnection`, a
`clientId` and the player's NetworkId. IDs come from a counter and are never reused within a run.
`NetworkIdComponent` is a `u32` in `net/Components.h`, on players for now. `NetworkIdMap` waits for E5.

### E3.13. Connecting from the client

`ConnectByIPAddress` does not block. The client prints "connecting" until the Connected event. On
ProblemDetectedLocally, it prints the reason and quits with an error.

### E3.14. The network systems

`AcceptClientsSystem`, `ServerReceiveSystem` and `ClientReceiveSystem`, in `game/systems`.

### E3.15. Two test programs

The `ByteWriter` and `ByteReader` round trip goes in a target with the standard library only, so it
runs on Win32 too. A `NetTests` target, x64 only, holds a server World and client Worlds in one
program. The Win32 configure stays free of network access.

### E3.16. `scripts/check-join.ps1`

The script starts the server, then client A, then client B, closes A, then B. The server prints
player counts 1, 2, 1, 0, B prints "player A left", and every process exits with no `[WARNING]` or
`[ERROR]` and 0 leaked blocks.

### E3.17. The server closes its clients before it stops

Added 2026-11-25, after the join test. `CloseListenSocket` closes every accepted connection
"ungracefully" (`isteamnetworkingsockets.h`, line 193), so no close reaches the client, and the client
waited 12.8 s for a timeout. `ServerApplication::CloseClients` runs after the loop in `Run`: it closes
every slot with the reason "server closed", then polls GNS for `kCloseWait`, 200 ms, so GNS's thread
sends the closes before the program ends. `ServerSocket::Close` passes linger as true, so GNS tries to
send queued reliable messages first (line 184).

- **Chose `Run` over `~ServerSocket` for the close,** because the socket does not know its
  connections; the `ClientTable` does.

### What was built

Built 2026-11-24 to 2026-11-26.

- **E3.1.** `vcpkg.json` at the repo root lists only `protobuf`, with `builtin-baseline` at the
  prototype's vcpkg commit `74f5ea45`. In the x64 branch of `CMakeLists.txt`, FetchContent pulls GNS
  with `SYSTEM`, after SDL. `USE_CRYPTO` is set in the cache with `FORCE`, because GNS declares it
  with `set(CACHE)` under CMake 3.15 policies, so a plain `set` loses on the first configure.
  `BUILD_SHARED_LIB`, `BUILD_EXAMPLES`, `BUILD_TESTS` and `BUILD_TOOLS` are off. `GameEngines5` links
  `GameNetworkingSockets::static`.
- **E3.2.** `VCPKG_ROOT` is `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\vcpkg`.
  Rider configures `build/cmake-x64` on its own. A terminal configure of the same folder at the same
  time broke GNS's BCrypt test and once its clone, so configure from the terminal in another folder.
- **E3.4.** `scripts\check-includes.ps1` checks every `#include` under `include/`, `src/` and
  `Main.cpp` against the module rules, prints each break as file:line, and exits 1. `utils` and
  `ecs` may include each other, because `Profiler.h` includes `ecs/WorldFrameStats.h`. It is not in
  `run-tests.bat` yet.
- **E3.5.** `net/NetworkLibrary`. It throws `std::runtime_error` with GNS's message when `Init` fails.
  `Main.cpp` starts with `#pragma init_seg(lib)` (warning 4073 off), and `LeakReportAtExit`'s
  destructor runs the leak report. This is needed because three global `std::vector`s in GNS's
  `csteamnetworkingsockets.cpp` (lines 145, 146 and 425) free their 848 bytes after `main` returns.
  `main`'s `std::exception` catch prints `[ERROR] `, so `run-game.ps1` counts it.
- **E3.6.** `net/ConnectionEvents.h` holds `ConnectionState`, `ConnectionEvent` and
  `ConnectionEventQueue`, with no GNS header. `PollConnectionEvents()` returns a span, and the
  applications append it to the World's `ConnectionEventQueue` only when the World holds one. The
  World's system empties the queue, so a frame with 0 Fixed steps loses no event.
  `ToConnectionState` returns `Other` after its switch, because MSVC warns C4715 without it. The
  side that closes its own connection still gets one event, Connected to None, so systems ignore
  events whose new state is None.
- **E3.8.** `net/ReceivedMessage.h`, `NET::ServerSocket` and `NET::ClientConnection`. Their `Receive`
  reads in batches of 64. `ClientConnection` throws `std::invalid_argument` for port 0, because GNS
  accepts an address with no port and connects to port 0. A second server on a busy port starts
  with no error: Windows lets an IPv4-only socket share a port with a dual-stack one.
- **E3.9.** `NET::kDefaultPort` is 27020, in `net/Ports.h`. `core/NetworkSettings.h` and
  `app/NetworkArguments`. `Application` holds `std::optional<NET::NetworkLibrary>` and fills it only
  for a client, so offline runs do not start GNS.
- **E3.10.** `net/ByteWriter`, `net/ByteReader` and `net/Messages.h`. `MessageType` starts at 1, so a
  zeroed byte is never a type. `kProtocolVersion` is 1. A failed read marks the reader invalid, and
  every later read fails too. `Send` before Connected succeeds: GNS holds the message until the
  connection comes up.
- **E3.11 to E3.14.** Built together, because none runs alone. Two changes from the tasks: the server
  spawns the player on a Hello, not on Connected, so the version check comes first; and a slot holds
  the player's NetworkId, not its Entity, because `CommandBuffer::CreateEntity` returns a placeholder
  that lasts only its pass. `AcceptClientsSystem` finds the player to despawn by querying
  `NetworkIdComponent`. `ServerSocket::Close(connection, reason)` calls `CloseConnection`, and GNS
  keeps a closed peer's handle until then (`steamnetworkingtypes.h`, lines 403 to 416).
- **E3.15.** `MessageTests` and `NetTests` have `add_test` lines, and `scripts\run-tests.bat` builds
  them. `NetTests` sits after the ECS targets in `CMakeLists.txt`, because it needs `ECS_SOURCES`.

### What was checked

x64 Debug and Release, 0 warnings and 0 errors on every build. After each task, `run-tests.bat`
passed in all four configurations, and `run-game.ps1 -Server -Scene 4 -Ticks 300` exited 0 with no
window and 0 leaked blocks.

**The build costs.**

| measure | result | command |
|---|---|---|
| cold configure, prototype vcpkg, empty binary cache | 323 s, against the prototype's 454 s; no line with "openssl" | `Measure-Command { cmake -S . -B build\check-3a ... }` |
| cold configure, VS 2026 and its bundled vcpkg | 264 s; Abseil 28 s, protobuf 2 min | vcpkg's "Elapsed time to handle" lines |
| configure with protobuf installed | 49 s; `--fresh` with every package installed: 23.5 s | not recorded |
| cold build | Debug 160 s, Release 159 s | not recorded |
| Win32 configure | 18.7 s, and it never runs vcpkg | not recorded |
| `GameNetworkingSockets_s.lib` | 27.5 MB Debug, 10.7 MB Release | file size |
| `libprotobuf.lib` | 231 MB, plus 91 Abseil libs, no DLL | `vcpkg_installed/x64-windows-static-md` |
| Release exe | 0.67 MB before GNS, 2.7 MB after | file size |
| exe DLLs | `WS2_32.dll`, `IPHLPAPI.DLL`, `bcrypt.dll`, no protobuf or Abseil | `dumpbin /dependents GameEngines5.exe` |

**The tests.**

- **`MessageTests`**, x64 and Win32, 29 checks: the high byte first, round trips of every width with
  0 and the maximums, a failed read that stays failed, the exact bytes of all four messages, their
  round trips, every cut-short Welcome, an extra byte, the wrong type, and `PeekMessageType` on an
  empty span and types 0, 5, 255 and 1 to 4.
- **`NetTests`**, x64 only, a server World and two client Worlds over loopback on port 27098, each
  built as `BossFightScene` builds it, 26 checks. A is client 1 with NetworkId 1, and B is client 2.
  A hears that player 2 joined, and B hears nothing about itself. Each client sends one Hello, though
  every World sees every event. The server holds 1, 2, 1, then 0 players, counted in both the
  `ClientTable` and the entities with a `NetworkIdComponent`. B hears that player 1 left. A Hello
  with version 2 is closed with "wrong protocol version" and spawns no player. With 8 clients in, a
  ninth is closed with "server full", and the 8 players stay.

**The runs.**

- **GNS's thread.** A Toolhelp snapshot counted 4 threads before `Init`, 5 after, still 5 with a
  listen socket and a loopback connection, and 4 after `Kill`. A connection with 200 reliable
  messages made 1,880 allocations, counted from both threads, and 0 leaked blocks at exit.
- **GNS off when offline.** Inside noise: boss fight windows after 4 s had 70 threads as a client,
  and 69, 69 and 70 offline. The GPU driver moves the count by about 1.
- **The flags.** `--server`, `--port`, `--connect` with and without a port each printed the right
  line. `--port 5`, `--server --connect x`, ports `0`, `70000` and `abc`, `--connect :27020`, a bare
  `--connect` and a bare `--port` each printed their own `[ERROR]` line and exited 1. `--server
  --scene 99` printed `[ERROR] no scene "99"` and exited 1.
- **`check-join.ps1`.** 15 of 15 checks. The server prints players 1, 2, 1, 0, 1; A hears that player
  2 joined, B that player 1 left; C prints "lost the server: server closed" 0.0 s after the server's
  exit and exits 1. Before E3.17, C waited 12.8 s.
- **What networking costs.** Release exe, 3 rounds each: a client offline for 12 s, the server alone
  for 1,200 ticks, and the server with two clients for 12 s. `ClientReceiveSystem` averages 0.001 ms a
  frame. `Record`, the frame average and process CPU time stay inside run-to-run noise. A connected
  client makes 112 to 118 more allocations than an offline one over the run, with peak live bytes
  2,952 higher; the server with two clients makes 204 to 206 more than alone, with a peak about 22 KB
  higher. Neither grows with frames or ticks.
- **The demo output.** `scripts/capture-output.sh v145`: all four outputs match `baselines/vulkan`
  line for line, so the move from `v143` to `v145` changed nothing the demo prints.

## E4. Input as unreliable messages

**Carried over from E3.** Raised while building E3, for this epic's design to settle:

- **Split `ServerReceiveSystem`.** Today it reads the socket, checks the version and spawns: three
  jobs. Only one system may call `ServerSocket::Receive`, because one call takes every waiting
  message. The likely shape: `ServerReceiveSystem` only sorts messages by type into per-type inbox
  resources, and one system per kind of work reads its inbox, `JoinSystem` for Hello and an input
  system for this epic. The client side gets the same split in E5.
- **How input finds its player.** An input message arrives with a connection number. With only the
  `ClientTable`, the server looks up the slot, then scans players by NetworkId. A
  `ConnectionComponent { connection, clientId }` on the player, as Overwatch does (its GDC 2017 ECS
  talk), makes it one query. The table stays for the 8-slot cap and for clients before their Hello.
- **Unreliable sends.** Both `Send` functions pass `k_nSteamNetworkingSend_Reliable` only. Input
  needs `k_nSteamNetworkingSend_Unreliable`: a second parameter, or a second function.
- **Spawning by role.** The server still spawns the offline Mario and its own boss, and each client
  spawns its own Mario and boss. The server's network players have no gun and no renderable. Which
  role spawns what is decided here or in E5.
- **Late joiners.** A client that joins late does not learn about the players already there.
  E5's snapshots fix that.
- **Lost inputs, from E2's design talk.** The server keeps a small buffer of unsimulated inputs and
  reuses the last one when an input is lost (transcript lines 674 to 689). The client sends every
  input since the server's last acknowledgment in each packet, so one lost packet costs nothing
  (lines 728 to 746).

## E5. Snapshots as unreliable messages

**Notes from ROADMAP.md, for the design talk.**

**Snapshots.** A fixed, hand-written layout: a header (tick, entity count), then per entity a network
ID, a kind byte, a position and an orientation, written field by field, never as a `memcpy` of a
struct with padding. The client turns the kind byte into its own mesh, because an `AssetStore`
handle depends on load order and differs between processes. Only one writer and one reader know the
byte order, so a generic layout can replace them later. A generic layout needs ROADMAP.md, ECS Hardening row 8,
stable asset IDs, and a way to send components that point at other entities. Row 8 does not cover
that last one: it makes a type ID match between processes, but an entity handle is a slot index in
creation order, with no name to hash, so each World numbers its entities differently. Only the
`NetworkId` matches on both sides. Two ways to use it, still to design: rewrite network IDs into
local handles as a packet loads, as EnTT's `continuous_loader` does, or store the network ID in the
component and look it up through `NetworkIdMap` on each access, as Unreal sends object references as
`FNetworkGUID`s.

**Open:** the tick rate, the send rate and the snapshot layout.

## E6. Interpolation

**Notes from ROADMAP.md, for the design talk.**

**Interpolation.** It shares the blend math with ROADMAP.md, Fixed step additions row 1: lerp the position and
scale, slerp the orientation, as one free function. The rest differs: the two states come from
snapshots, `t` comes from the render time between two server ticks, and a lost snapshot must be held
or guessed forward. Fixed step row 1 is not needed first, but it is a small warm-up that builds the
blend function and its test.

## E7. Shooting

## E8. The lag test

**Open:** how to measure ping.
