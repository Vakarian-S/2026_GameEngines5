# Build and test

**Last updated:** 2026-10-28.

How the repository is laid out, how to build it, and how to run its checks. Taking MemoryMonitor
into another project is in tools/memory-monitor.md.

## Layout

Eight programs and one library.

| Target | Kind | Holds |
|---|---|---|
| `GameEngines5` | program | `Main.cpp`, and everything under `src/` and `include/` |
| `MemoryMonitorDemo` | program | `demo/MemoryMonitorDemo.cpp`, a walk through every path the monitor watches |
| `MemoryMonitorTests` | program | `tests/MemoryMonitorTests.cpp`, the monitor's checks |
| `EcsTests` | program | `tests/EcsTests.cpp`, the ECS checks |
| `ArenaTests` | program | `tests/ArenaTests.cpp`, the arena's checks |
| `PhysicsTests` | program | `tests/PhysicsTests.cpp`, the physics math's checks: the ray, the quadratic solver, the ray-shape tests, and the rigid-body math |
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

The presets are `x64` and `Win32`, and the build and test presets add the configuration:
`x64-Debug`, `x64-Release`, `Win32-Debug`, `Win32-Release`.

`CMakePresets.json` pins the generator, the toolset and the build directory. The toolset is
pinned on purpose. A machine with more than one set of Build Tools installed will otherwise take
whichever is newest, and the saved output snapshots were built with `v143`.

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

- `external/MathLibrary/` - the course math library, 48 files
- `external/glm/` - glm 0.9.9, which the math library's `DebugUtlities` needs

Neither folder came with a license file. Add glm's before this repository goes anywhere public.

## Running the tests

From the project root in PowerShell:

```powershell
.\scripts\run-tests.bat
```

This builds the `MemoryMonitorTests`, `EcsTests`, `ArenaTests` and `PhysicsTests` targets with CMake and runs them
through CTest in Debug and Release, for x64 and Win32. It configures a build tree first if there is not one already. CTest
prefixes each line of the program's own output with `1:`.

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

`EcsTests`, `ArenaTests` and `PhysicsTests` are their own programs, registered with CTest next to
`MemoryMonitorTests`, so `ctest --preset x64-Debug` runs all four. `EcsTests` runs 220 checks in
every configuration (2026-11-05), 5 of them for `FixedStep`, which is core but needs nothing else. `ArenaTests` runs 73 in Debug and 72 in Release, because the check that reads
the `0xDD` fill only exists in Debug.

`PhysicsTests` compiles the `PHYSICS` math files and links `MathLibrary`, with no ECS, SDL or
Vulkan, so it builds for Win32 too. It runs 202 checks in every configuration (2026-11-03): the ray,
`SolveQuadratic` (including a touching root that float rounding pushes below 0), ray against
sphere, box, can and capsule, `ToLocalRay`, `MakePickRay`, and from 4c the forces, drag, position,
orientation, inertia per shape, grip, the spin limit and `MouseConstraint`. Floats are compared with `Near`, within `1e-4`.

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
