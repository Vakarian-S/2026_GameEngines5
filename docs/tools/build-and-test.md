# Build and test

**Last updated:** 2026-11-26.

How the repository is laid out, how to build it, how to run its checks, and how to run the game from
a script. Taking MemoryMonitor into another project is in tools/memory-monitor.md.

## Layout

Eleven programs and one library.

| Target | Kind | Holds |
|---|---|---|
| `GameEngines5` | program | `Main.cpp`, and everything under `src/` and `include/`. Links GNS (GameNetworkingSockets). x64 only |
| `MemoryMonitorDemo` | program | `demo/MemoryMonitorDemo.cpp`, a walk through every path the monitor watches |
| `MemoryMonitorTests` | program | `tests/MemoryMonitorTests.cpp`, the monitor's checks |
| `EcsTests` | program | `tests/EcsTests.cpp`, the ECS checks |
| `ArenaTests` | program | `tests/ArenaTests.cpp`, the arena's checks |
| `PhysicsTests` | program | `tests/PhysicsTests.cpp`, the physics math's checks: the ray, the quadratic solver, the ray-shape tests, the rigid-body math, and the per-axis scale math |
| `GpuTests` | program | `tests/GpuTests.cpp`, the checks for Vulkan classes that need a real GPU: today `CubeMap`. x64 only |
| `MessageTests` | program | `tests/MessageTests.cpp`, the byte layout of the network messages: `ByteWriter`, `ByteReader` and `Messages`, with no GNS |
| `NetTests` | program | `tests/NetTests.cpp`, a server World and two client Worlds joining and leaving over loopback. Links GNS. x64 only |
| `ArenaDemo` | program | `demo/ArenaDemo.cpp`, the same work on the heap and from an arena |
| `EcsBench` | program | `bench/EcsBench.cpp`, a load for the profiler |
| `MathLibrary` | static library | `external/MathLibrary/include/` |

`EcsBench` and `ArenaDemo` are not tests. They measure rather than check, so CTest never launches
them. Run them by hand:

```powershell
.\bin\x64\Release\EcsBench.exe
.\bin\x64\Release\ArenaDemo.exe
```

Release, always. Debug is roughly fifteen times slower and says nothing about a shipped build.

Each program has its own `main()`, so each one is its own target. `cmake/CommonSettings.cmake`
holds what they share: C++20, the warning level, the include paths, and where output lands.

`MathLibrary` is a library because it is someone else's code, vendored under `external/`.

`MemoryMonitor` is not a target. It is two files, and each program that wants it compiles them,
which is the same thing this README tells anyone else to do with it. The engine picks them up
with the rest of `src/`; the demo and the tests name `src/utils/MemoryMonitor.cpp` on their own,
because they want the monitor and not the renderer.

Everything under `src/` and `include/` belongs to the engine, so adding a file there needs no
change to `CMakeLists.txt`. The build re-reads those folders each time.

Output goes to `bin/<platform>/<configuration>/`.

### Building

CMake, one build tree per platform, each holding both configurations:

```powershell
cmake --preset x64
cmake --build --preset x64-Debug
```

The engine target is x64 only: the Vulkan SDK ships no 32-bit `vulkan-1.lib`, so
`find_package(Vulkan)` fails on Win32. The presets are `x64` and `Win32`, and the build and test presets add the configuration:
`x64-Debug`, `x64-Release`, `Win32-Debug`, `Win32-Release`.

`CMakePresets.json` pins the generator (`Visual Studio 18 2026`), the toolset (`v145`) and the
build directory. The toolset is pinned on purpose: vcpkg builds protobuf with the newest compiler on
the machine, and the engine must link with the same one. The toolset was `v143` until 2026-11-24, so
output snapshots saved before then came from `v143`.

### Setting up a machine for x64

The x64 build needs GNS, and GNS needs protobuf, which vcpkg builds. Before the first x64 configure:

1. Install Visual Studio Build Tools 2026 with the C++ workload and the "vcpkg package manager"
   component.
2. Set the `VCPKG_ROOT` environment variable to that vcpkg folder, then open a new terminal.

The x64 preset loads vcpkg's toolchain file from `VCPKG_ROOT`, and `CMakeLists.txt` stops with a
clear error when it is missing. `vcpkg.json` lists only `protobuf`. The preset sets both vcpkg
triplets to `x64-windows-static-md`, so protobuf and Abseil link into the exe and ship no DLL. The
first cold x64 configure takes about 7.6 min, mostly vcpkg building protobuf and Abseil; later
configures reuse them. The Win32 preset uses no vcpkg and needs no network access.

### Opening it in an IDE

Open `CMakeLists.txt`, not a solution file. Rider and Visual Studio both read
`CMakePresets.json`, so the profile list offers `x64` and `Win32` already filled in. Pick one of
those rather than letting the IDE invent a profile of its own: left to choose, it picks its own
build directory and whichever compiler is newest, and then a demo run out of the IDE can differ
from `scripts/capture-output.sh` for no visible reason.

`build/cmake-x64/GameEngines5.sln` is generated, so Visual Studio has a solution to open if you
want one. Never change project settings inside it. It is rewritten on every reconfigure, and it
is not in version control. `CMakeLists.txt` is the file to edit.

### The vendored libraries

`external/` holds two libraries copied in whole, so the repository builds on any machine with
Visual Studio and nothing else set up first:

- `external/MathLibrary/` - Humber's math library, 48 files
- `external/glm/` - glm 0.9.9, which the math library's `DebugUtlities` needs

Neither folder came with a license file. Add glm's before this repository goes anywhere public.

## Running the tests

From the project root in PowerShell:

```powershell
.\scripts\run-tests.bat
```

This builds the `MemoryMonitorTests`, `EcsTests`, `ArenaTests`, `PhysicsTests` and `MessageTests`
targets with CMake and runs them through CTest in Debug and Release, for x64 and Win32. On x64 it
also builds and runs `GpuTests` and `NetTests`. It configures a build tree first if there is not one
already. CTest prefixes each line of the program's own output with `1:`.

To run the tests without the script, against a tree that is already configured:

```powershell
ctest --preset x64-Debug
```

To run one configuration only:

```powershell
.\scripts\run-tests.bat x64 Debug
```

Red `Refused:`, `Mismatch:` and `Damaged:` lines are expected. Those tests make mistakes on purpose. Only `FAIL` means something broke.

`MemoryMonitorTests` is the one target built without `/sdl`, and it has to be. `/sdl` overwrites a
pointer with a junk address the moment it is deleted. The double free test deletes the same pointer
twice on purpose, so under `/sdl` the second delete hands the monitor a wild address instead of a
stale one, and reading the header there is an access violation. `CMakeLists.txt` says the same thing
next to the setting.

Each test starts with a `TEST CASE:` header and one plain line saying what it checks. Debug runs 81 checks. Release runs 42, because the checks that need a check value or the leak list are skipped there.

`EcsTests`, `ArenaTests`, `PhysicsTests` and `MessageTests` are their own programs, registered with
CTest next to `MemoryMonitorTests`, so `ctest --preset Win32-Debug` runs all five, and
`ctest --preset x64-Debug` runs those five, `GpuTests` and `NetTests`. `EcsTests` runs 348 checks in
every configuration (2026-11-13). Some cover core code that needs nothing else: 5 for `FixedStep`,
the frame time stats, 11 for the GPU timestamp math in `core/TimestampMath.h`, and 11 for
`AlignUp` in `core/AlignUp.h`. `ArenaTests` runs 73 in Debug and 72 in Release, because the check that reads
the `0xDD` fill only exists in Debug.

`PhysicsTests` compiles the `PHYSICS` math files and links `MathLibrary`, with no ECS, SDL or
Vulkan, so it builds for Win32 too. It runs 218 checks in every configuration (2026-11-14): the ray,
`SolveQuadratic` (including a touching root that float rounding pushes below 0), ray against
sphere, box, can and capsule, `ToLocalRay`, `MakePickRay`, and from 4c the forces, drag, position,
orientation, inertia per shape, grip, the spin limit and `MouseConstraint`, and from the non-uniform
scale work the per-component math in `core/VectorMath.h`, a box scaled on one axis, per-axis
inertia and `NormalMatrix`. Floats are compared with `Near`, within `1e-4`.

`GpuTests` needs a GPU and a desktop session, because it opens a hidden SDL window to give
`GPUContext` its surface. Only the x64 tree builds Vulkan, so it is x64 only. While a check runs, the
test sends `std::cerr` into a string, and a validation `[WARNING]` or `[ERROR]` in it fails the check.
Release has no validation layers, so there the same checks only test what the code returns and
throws. It runs 9 checks in both configurations (2026-11-17): one folder of six 4x4 faces loads
cleanly, and a face of the wrong size, a face that is not square and a missing face each throw with
the file's name. Its images live in `tests/assets/cubemaps/`, and a POST_BUILD step copies them next
to the executable as `tests/assets/...`.

`MessageTests` compiles only `ByteWriter`, `ByteReader` and `Messages`, which include no GNS header,
so it builds for Win32 too and checks the byte layout on both platforms. It runs 29 checks in every
configuration (2026-11-25): round trips of every width, a read past the end returning 0 and staying
failed, the exact bytes of all four step 3 messages, cut-short, too-long and wrong-type messages, and
`PeekMessageType`.

`NetTests` runs a server World and two client Worlds in one program, over loopback on port 27098,
each built the way `BossFightScene` builds it. Every World gets every connection event, as the
applications hand them over, and each World's prints go to its own log, so the checks read what
each side said. It runs 26 checks (2026-11-26): both clients are welcomed with the right ids, each
hears about the other, the server's player count follows 1, 2, 1, 0, and each client sends one
Hello though every World sees every event. A bare `ClientConnection` with no World sends a Hello
with version 2 and is closed with "wrong protocol version", and a ninth client, while 8 are in, is
closed with "server full"; neither gets a player or keeps a slot. It links GNS, so it is x64 only, and it fails if
something else holds port 27098.

Four of the tests start threads: they hammer the counters and the leak list from 4 threads at once, ask for reports while that happens, and race two frees on the same block. They repeat their work 3 times per run. Timing bugs do not show up every run, so it is worth running the tests a few times in a row.

## Checking that a refactor changed nothing

`scripts/capture-output.sh` builds all four configurations of `MemoryMonitorDemo`, runs them, and
saves the output. It replaces heap addresses with `0xADDR`, so runs can be compared.

One thing to know: in Debug the demo prints its own file name and line numbers, from `MEMMON_NEW`
and `MEMMON_MAKE_UNIQUE`. Add or remove a line in `demo/MemoryMonitorDemo.cpp` and those lines shift,
even when nothing else changed. Release prints no file tags, so it stays a clean comparison.

```bash
./scripts/capture-output.sh step0          # before the change
./scripts/capture-output.sh step1 step0    # after, and compare
```

## Running the game from a script

`scripts/run-game.ps1` starts `GameEngines5.exe`, plays a list of steps into its window, closes it the
way the X button does, and prints a summary. It checks a change that shows on screen without a
person at the keys.

```powershell
.\scripts\run-game.ps1 -Scene 0 -Steps "press F2","wait 1500","shot scene1"
.\scripts\run-game.ps1 -Steps (1..20 | ForEach-Object { "press F2", "wait 700", "press F1", "wait 700" })
.\scripts\run-game.ps1 -Steps "wait 500","click 540 390","wait 800","shot picked"
.\scripts\run-game.ps1 -Server -Scene 3 -Ticks 600
```

| parameter | meaning | default |
|---|---|---|
| `-Scene` | the scene row, passed to the exe as `--scene N` | none, which starts the last scene |
| `-Steps` | the steps, one per string | none |
| `-Config` | `Debug` or `Release` | `Debug` |
| `-OutDir` | where the logs and screenshots go | `%TEMP%\ge5-run` |
| `-StartWait` | milliseconds to wait for the window | 4000 |
| `-Server` | runs the headless server, passing `--server` and `--ticks N` | off |
| `-Ticks` | the server's tick limit, 60 ticks a second | 600 |

The script runs whatever exe is in `bin`, and `run-tests.bat` does not rebuild the game, so run
`cmake --build` first. The boss fight needs `-StartWait 6000`: with the default, the first held keys
were sometimes lost while the window took focus.

With `-Server`, the script skips `-Steps` and `-StartWait`. It waits for the server to exit, and kills
it if it is still running 30 s after its ticks should end. The summary adds whether a window opened,
the `tick N` lines (the first and the last) and the "Ran for" line.

The exe's own flags are `--server` (headless), `--scene N` (the starting row), `--ticks N` (the
server stops after N ticks), `--port P` (the server's port, 27020 by default) and
`--connect host[:P]` (start as a client of that server). Only scene 4, the boss fight, has
networking. Ctrl+Q stops the server from a real console. A pipe, such as Rider's run
window or this script, cannot send it, so an IDE run configuration for the server should open an
external console.

| step | does |
|---|---|
| `press KEY` | taps a key for 50 ms |
| `hold KEY[,KEY] MS` | holds one or more keys together, for example `hold W,Shift 600` |
| `wait MS` | waits |
| `shot NAME` | saves the window as `NAME.png` in the output folder |
| `move DX DY [COUNT]` | moves the mouse by DX, DY, COUNT times, 16 ms apart |
| `rightdown`, `rightup`, `leftdown`, `leftup` | presses or releases a mouse button |
| `click X Y` | left-clicks at X, Y inside the window, counted from its top-left corner |
| `resize W H` | moves the window to 50, 50 and sizes it W by H |
| `minimize`, `maximize`, `restore` | the window states |

Keys are F1 to F5, A to Z, 0 to 9, Shift, Ctrl, Alt, Enter, Space, Escape and the four arrows (Up,
Down, Left, Right). A combination is a
hold with the modifier first: `hold Ctrl,P 100`, `hold Alt,Enter 100`.

To run on the AMD 610M, set `$env:VK_LOADER_DRIVERS_SELECT = "*amd*"` before the script and remove
it after. The Vulkan loader then hides the NVIDIA driver, and the engine takes the first GPU that
fits. In Debug the loader prints one `[WARNING]` for the hidden driver.

Frame-time runs on the 4060 need the NVIDIA driver's power saving off. With the default setting, a
light scene lets the driver drop the GPU to a slow power state and switch states while the game runs,
and frames miss their refresh (dev-notes/frame-cap-and-timer.md, step 5). For such a run, set NVIDIA Control Panel →
Manage 3D Settings → Program Settings → `bin\x64\Release\GameEngines5.exe` → Power management mode →
Prefer maximum performance. Set it back to the default afterwards; the machine keeps the default
setting between runs.

The summary gives the exit code, the counts of `[WARNING]` and `[ERROR]` lines and failed asserts
(with the first five), how many scenes closed, the leaked blocks, and each "fell to the heap" count. A
clean run reads exit code 0, zero of each, 0 leaked blocks and 0 fell to the heap. The full output is
in `out.txt` and `err.txt` in the output folder.

How it works:

- Keys go through Windows' `SendInput` as scancodes, because SDL reads keys by scancode. The mouse
  goes through the same call.
- Windows sends input only to the active window, so the script focuses the game before each step.
- Screenshots copy the window's area of the screen. The script turns on DPI awareness first, or the
  area comes out at the wrong size.
- The close is a real close request, so the game shuts down normally and prints its leak report.
- The script sets `GE5_NO_DIALOGS=1` for the game. `main` then turns off the Debug runtime's assert
  and abort windows and the Windows crash window, so a failed assert prints its line to `err.txt`
  and the game exits with code 3 instead of waiting for a click. A run from Rider leaves it unset
  and keeps the window, with Retry into the debugger.
- If the game exits early, the script stops, says before which step, and still prints the summary.

Limits:

- It needs the desktop. The game takes focus while it runs, so a click elsewhere can steal a step. A
  cloud session cannot run it.
- Timing is wall-clock time, so a held key covers a different number of frames on each run.
- Screenshots show the screen, so another window or a DPI change alters them.

ROADMAP.md, "Automated game tests", moves this inside the engine.

## Checking joining and leaving

`scripts/check-join.ps1` runs a real server and three real clients and reads their logs. Build x64
first; it runs whatever exe is there.

```powershell
.\scripts\check-join.ps1
.\scripts\check-join.ps1 -Config Release -OutDir C:\temp\join
```

It starts the server with `--ticks 1800`, client A at 2 s and client B at 9 s, closes A at 16 s and B
at 19 s, starts client C at 21 s, and lets the server stop on its own at about 30 s. It passes when:

- the server prints players 1, 2, 1, 0, 1
- A prints "player 2 joined", and B prints "player 1 left"
- C prints "lost the server: server closed" within 2 s of the server's exit, and exits 1
- the server, A and B exit 0
- all four show 0 leaked blocks and no `[WARNING]`

It opens three game windows for about 45 s, so leave the desktop alone while it runs. The logs land
in `-OutDir`, `%TEMP%\ge5-check-join` by default, one `.out` and one `.err` per process. The first
run passed 15 of 15 checks (2026-11-25).

## Checking the include rules

`scripts/check-includes.ps1` reads every `#include` under `include/` and `src/` and checks it against
STATE.md's architecture rules. Each module may include only the modules its table row lists; the
GNS headers (`<steam/...>`) may appear only in `net`, and Vulkan's only in `vulkan`; and a short list
of files, such as `ByteWriter` and `Messages`, must include neither, so targets without GNS can
compile them. It prints each broken include as file:line and exits 1, or exits 0 when every include
follows the rules. The rules sit in tables at the top of the script, so a new rule is one line.

```powershell
.\scripts\check-includes.ps1
```

`run-tests.bat` does not call it yet. Run it after a change that adds includes.

## Files

- `scripts/run-tests.bat`: builds and runs the five test programs, and `GpuTests` and `NetTests` on
  x64.
- `tests/GpuTests.cpp` and `tests/assets/cubemaps/`: the GPU checks and their face images.
- `tests/MessageTests.cpp`: the network messages' byte layout. `tests/NetTests.cpp`: joining and
  leaving over loopback.
- `scripts/capture-output.sh`: the refactor check above.
- `scripts/run-game.ps1`: runs the game, or with `-Server` the headless server, from a script.
- `scripts/check-join.ps1`: a real server and three clients joining and leaving.
- `scripts/check-includes.ps1`: the include rules.
- `vcpkg.json`: the packages vcpkg builds for x64, today only `protobuf`.
