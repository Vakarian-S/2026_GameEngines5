# State

**Last updated:** 2026-11-11.

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
| Physics course work, moved into this engine | done 2026-11-03 | dev-notes/dangle-physics.md || Concurrency, controller, audio, file I/O, DLLs, frustum culling, networking | not started | ROADMAP.md, "Course topics" |

## What exists

| part | what it does | where | details |
|---|---|---|---|
| ECS | Archetype storage in fixed-size chunks (16 KB by default) from the World's `ChunkPool`. Each chunk holds every column for a range of rows. Systems loop with `Query::ForEachChunk`, which hands them one `ChunkView` per chunk. `CreateEntity(values...)` puts a new entity straight in its final archetype, in the World and through the command buffer. `World::Clear` empties every archetype in one call and keeps the archetypes, so a respawn reuses their chunks. Dead entity slots form a free list inside the slot table, linked through each dead slot's `row`. A `DEBUG::DebugWatch` tag stops the debugger in every system that reaches the tagged entity, or in one named system. Queries with a cached match list, a command buffer for structural changes, per-frame counters (`WorldFrameStats`), chunk takes and returns included. Single threaded | `include/ecs` | ECS.md; dev-notes/ecs-chunks.md, create-with-components.md, clear-the-world.md, intrusive-free-list.md, entity-breakpoints.md |
| Systems and phases | Each scene owns a `SystemRegistry` and adds its systems in `OnCreate`. Phases run in order: Fixed (a 1/60 s `FixedStep`, 0 or more times a frame), Update, Extract. The command buffer plays back after each phase. `SetEnabled` switches a system off | `include/ecs`, `include/core/FixedStep.h` | ECS.md, dev-notes/systems-and-phases.md |
| Components and systems | Split by module, each with a `Components.h` and a `systems/` folder: `core` (transform, drawing), `physics` (motion, colliders, picking), `game` (Scene0's test load, and the Pacing Scene's row tags, `SlideRightSystem` and `LoadSystem`) | `include/core`, `include/physics`, `include/game` | the headers |
| Transform | One component: position, uniform scale, a quaternion orientation. `ModelMatrix` is the one place translate × rotate × scale is built | `include/core/Components.h` | dev-notes/transform.md |
| Input | `InputState`: keys held and pressed, mouse buttons held and pressed, the mouse position and the window size. Systems read it through `SystemContext` | `include/core/InputState.h` | dev-notes/physics-scene.md |
| Renderer | Vulkan. Two pipelines from `PipelineSettings`: lit meshes (Phong, up to 4 lights, a material per draw) and debug lines (one draw for all of them). The scene hands over a camera, lights, a draw list and debug lines, and never sees a Vulkan type. The present mode is a setting (`include/core/PresentMode.h`): FIFO by default, or FIFO_RELAXED, MAILBOX or IMMEDIATE when the GPU offers them, with FIFO as the fallback. A mode change or a window resize rebuilds the swapchain at the start of the next frame | `include/vulkan`, `include/core/PresentMode.h` | dev-notes/vulkan-renderer.md, dev-notes/present-mode-and-fullscreen.md |
| Debug lines | Lines, wire boxes, spheres, cylinders and capsules, plus the grid and axes in every scene (G toggles them) | `include/core/debug` | dev-notes/line-pipeline.md |
| Resources | `ResourceStore` holds meshes, textures and materials behind handles, cached by path or by value, freed only at exit. Scenes load through the `ResourceLoader` interface | `include/core/ResourceLoader.h`, `include/vulkan` | dev-notes/scene-switcher.md |
| Scenes | `Scene0`: 1,000 moving cubes, the benchmark for `Record`. `Scene1`: a lit, turning Mario between two cubes. `PhysicsScene`: six shapes with collider wires to pick and dangle from the mouse. `PacingScene`: two rows of 10 Marios that slide and wrap, the top row moved per frame and the bottom per fixed step, with Ctrl+H cycling a fake CPU load. F1 to F4 switch; C toggles wires, M meshes, and R in `PhysicsScene` clears the World and respawns the shapes | `include/scenes` | dev-notes/physics-scene.md, dev-notes/dangle-physics.md, dev-notes/live-fps-and-pacing-scene.md |
| Camera | A fly camera in every scene: WASD, Q and E, Shift, right mouse to look. Stores a position, yaw and pitch, so it never drifts | `include/core/FlyCameraController.h` | dev-notes/fly-camera.md |
| App | `Application` owns the window, renderer, `SceneManager`, timer, frame cap, frame time stats and frame arena, and runs the loop. App keys: F1 to F4, G, Ctrl+P (next present mode), Ctrl+L (next frame cap), Alt+Enter (borderless fullscreen). `GameEngines5.exe 0` starts in Scene0; no argument starts in the last scene | `include/app` | dev-notes/scene-switcher.md |
| Frame timing | `Timer` counts nanoseconds with `SDL_GetTicksNS`. `FrameCap` holds a cap (off, 30, 60, 100, half the refresh rate, refresh minus 3) by sleeping with `SDL_DelayNS` to 1 ms before a fixed deadline, then spinning. `FrameTimeStats` sums the frame times once a second, and `Application` puts the line `FIFO \| cap off \| 239.8 fps \| 4.17 ms (min, max)` in the window title and the console. With no cap, FIFO and the display pace the loop | `include/core/Timer.h`, `FrameCap.h`, `FrameTimeStats.h` | dev-notes/frame-cap-and-timer.md, dev-notes/live-fps-and-pacing-scene.md |
| Physics | Ray picking against four collider shapes, and rigid bodies held by a mouse constraint with inertia per shape. Plain free functions in `PHYSICS`, called by the systems | `include/physics` | dev-notes/dangle-physics.md; picking in dev-notes/physics-scene.md |
| Tools | `MemoryMonitor`, `Profiler`, `Arena` | `include/utils` | tools/ |
| Tests | `MemoryMonitorTests`, `EcsTests` (with `FixedStep` and `FrameTimeStats`), `ArenaTests`, `PhysicsTests`, in x64 and Win32, Debug and Release | `tests/` | tools/build-and-test.md |

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
5. **A new archetype row is zero bytes,** and no constructor runs. Every add or create of a component
   with data hands over its bytes; `T{}` is the default. ECS.md, "Rules of this engine".
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
