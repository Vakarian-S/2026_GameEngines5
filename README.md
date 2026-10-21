# GameEngines5

A small C++20 game engine, built step by step for a semester of coursework.

## What is in it

- **An archetype ECS.** Entities with the same components share tightly packed arrays, and systems
  loop over those arrays. Structural changes are queued in a command buffer and applied at fixed
  points.
- **A Vulkan 1.3 renderer**, written by hand: dynamic rendering, Phong lighting with up to 4
  lights, textures and materials, and OBJ models. The engine side never sees a Vulkan type.
- **Three tools:**
  - `MemoryMonitor` watches every allocation and free, and reports leaks.
  - `Profiler` times named blocks of code and counts the ECS work they cause.
  - `Arena` hands out memory from one block instead of asking the heap again and again.

## Layout

| Target | What it is |
|---|---|
| `GameEngines5` | the engine: `Main.cpp`, `src/` and `include/` |
| `MemoryMonitorTests`, `EcsTests`, `ArenaTests` | the test programs, under `tests/` |
| `MemoryMonitorDemo`, `ArenaDemo` | demos, under `demo/` |
| `EcsBench` | a benchmark load for the profiler, under `bench/` |
| `MathLibrary` | the course math library, vendored under `external/` with glm 0.9.9 |

Programs build to `bin/<platform>/<configuration>/`.

## Building

Windows and Visual Studio with CMake. From the project root:

```powershell
cmake --preset x64
cmake --build --preset x64-Debug
```

The presets are `x64` and `Win32`. The build presets add the configuration: `x64-Debug`,
`x64-Release`, `Win32-Debug` and `Win32-Release`. In Rider or Visual Studio, open
`CMakeLists.txt` and pick one of these presets.

## Running

```powershell
.\bin\x64\Release\GameEngines5.exe
```

Run benchmarks and demos in Release, such as `.\bin\x64\Release\EcsBench.exe`. Debug is much
slower and says nothing about a shipped build.

## Tests

```powershell
.\scripts\run-tests.bat
```

This builds the three test programs and runs them in Debug and Release, for x64 and Win32. Add a
platform and a configuration to run only one, such as `.\scripts\run-tests.bat x64 Debug`. Some
tests make mistakes on purpose and print red lines; only `FAIL` means something broke.

## Docs

A short doc for each tool lives in `docs/tools/`.
