# State

**Last updated:** 2026-11-05.

What the engine has today, and the architecture rules that hold everywhere. Read this first.

## Contents

- [The semester topics](#the-semester-topics)
- [What exists](#what-exists)
- [Architecture rules](#architecture-rules)
- [How to build and test](#how-to-build-and-test)

## The semester topics

| topic | status | details |
|---|---|---|
| Memory Monitor | done | tools/memory-monitor.md |
| Memory Manager (arena) | done 2026-10-01 | tools/arena.md, dev-notes/memory-manager.md |
| Profiling | done 2026-09-30 | tools/profiler.md, dev-notes/profiling.md |
| Vulkan rendering | done 2026-10-20, all 8 milestones | dev-notes/vulkan-renderer.md |
| Physics course work, moved into this engine | done 2026-11-03 | dev-notes/dangle-physics.md |
| Concurrency, controller, audio, file I/O, DLLs, frustum culling, networking | not started | ROADMAP.md, "Course topics" |

## What exists

| part | what it does | where | details |
|---|---|---|---|
| ECS | Archetype storage in fixed-size chunks (16 KB by default) from the World's `ChunkPool`. Each chunk holds every column for a range of rows. Systems loop with `Query::ForEachChunk`, which hands them one `ChunkView` per chunk. Queries with a cached match list, a command buffer for structural changes, per-frame counters, chunk takes and returns included. Single threaded | `include/ecs` | ECS.md |
| Systems and phases | Each scene owns a `SystemRegistry` and adds its systems in `OnCreate`. Phases run in order: Fixed (a 1/60 s `FixedStep`, 0 or more times a frame), Update, Extract. The command buffer plays back after each phase. `SetEnabled` switches a system off | `include/ecs`, `include/core/FixedStep.h` | ECS.md, dev-notes/systems-and-phases.md |
| Components and systems | Split by module, each with a `Components.h` and a `systems/` folder: `core` (transform, drawing), `physics` (motion, colliders, picking), `game` (Scene0's test load) | `include/core`, `include/physics`, `include/game` | the headers |
| Transform | One component: position, uniform scale, a quaternion orientation. `ModelMatrix` is the one place translate × rotate × scale is built | `include/core/Components.h` | dev-notes/transform.md |
| Input | `InputState`: keys held and pressed, mouse buttons held and pressed, the mouse position and the window size. Systems read it through `SystemContext` | `include/core/InputState.h` | dev-notes/physics-scene.md |
| Renderer | Vulkan. Two pipelines from `PipelineSettings`: lit meshes (Phong, up to 4 lights, a material per draw) and debug lines (one draw for all of them). The scene hands over a camera, lights, a draw list and debug lines, and never sees a Vulkan type | `include/vulkan` | dev-notes/vulkan-renderer.md |
| Debug lines | Lines, wire boxes, spheres, cylinders and capsules, plus the grid and axes in every scene (G toggles them) | `include/core/debug` | dev-notes/line-pipeline.md |
| Resources | `ResourceStore` holds meshes, textures and materials behind handles, cached by path or by value, freed only at exit. Scenes load through the `ResourceLoader` interface | `include/core/ResourceLoader.h`, `include/vulkan` | dev-notes/scene-switcher.md |
| Scenes | `Scene0`: 1,000 moving cubes, the benchmark for `Record`. `Scene1`: a lit, turning Mario between two cubes. `PhysicsScene`: six shapes with collider wires to pick and dangle from the mouse. F1 to F3 switch; C toggles wires, M meshes | `include/scenes` | dev-notes/physics-scene.md, dev-notes/dangle-physics.md |
| Camera | A fly camera in every scene: WASD, Q and E, Shift, right mouse to look. Stores a position, yaw and pitch, so it never drifts | `include/core/FlyCameraController.h` | dev-notes/fly-camera.md |
| App | `Application` owns the window, renderer, `SceneManager`, timer and frame arena, and runs the loop. `GameEngines5.exe 0` starts in Scene0; no argument starts in the last scene | `include/app` | dev-notes/scene-switcher.md |
| Physics | Ray picking against four collider shapes, and rigid bodies held by a mouse constraint with inertia per shape. Plain free functions in `PHYSICS`, called by the systems | `include/physics` | dev-notes/dangle-physics.md; picking in dev-notes/physics-scene.md |
| Tools | `MemoryMonitor`, `Profiler`, `Arena` | `include/utils` | tools/ |
| Tests | `MemoryMonitorTests`, `EcsTests`, `ArenaTests`, `PhysicsTests`, in x64 and Win32, Debug and Release | `tests/` | tools/build-and-test.md |

## Architecture rules

1. **Layers.** Engine code never names a render backend's types. A backend may use core types. Only
   `app` sees every layer, and only `main` includes `app`. This keeps a second backend possible.
2. **Modules depend downward only.** `physics` may include `core`, `game` may include both, and `core`
   never includes `physics` or `game`.
3. **The ECS depends on the standard library only.** Its headers name outside types (`InputState`,
   `DrawItem`, `LineVertex`) by forward declaration, so the ECS-only targets build without SDL, on
   Win32 too.
4. **Components are plain, trivially copyable data,** because a migration is a `memcpy`. The math types
   default their copies, so a component may hold them; a hand-written copy breaks `Register<T>()` at
   compile time.
5. **A new archetype row is zero bytes,** and no constructor runs. Every add of a component with data
   hands over its bytes; `T{}` is the default. ECS.md, "Rules of this engine".
6. **Structural changes go through the command buffer.** They apply at the end of the phase, so later
   phases see them and the same phase never does.
7. **A system gets everything through `SystemContext`,** never from the scene.
8. **Input enters in one place.** `Application` fills `InputState` once per frame. Read presses in
   Update, not Fixed; held state is safe in any phase.
9. **Math is free functions on plain values.** Systems loop, call and write the result, so the tests run
   the math with no ECS.
10. **Scale is uniform.** The normal matrix (the model's 3x3 part, no inverse) and the picking math
    depend on it.
11. **The ECS house rules** from Unreal Mass: ECS.md, appendix.

## How to build and test

- Build: `cmake --build --preset x64-Debug`
- Tests: `scripts\run-tests.bat`

tools/build-and-test.md has the layout, the IDE setup and the rest.
