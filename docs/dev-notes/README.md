# Dev notes

One note per finished feature: what was decided, why, and what it measured. Nobody updates these.

| note | what it holds | finished |
|---|---|---|
| profiling.md | how the profiler was built, and what it measured | 2026-09-30 |
| memory-manager.md | why the arena was built, and what it measured | 2026-10-01 |
| vulkan-renderer.md | all 8 renderer milestones, every decision and measurement | 2026-10-20 |
| scene-switcher.md | the scene switcher, `ResourceLoader` and its cache, `SceneManager` and `Application` | 2026-10-21 |
| fly-camera.md | why the fly camera stores angles, not a dual quaternion: the drift test and the options weighed, plus the input and wiring of step 1b | 2026-10-22 |
| line-pipeline.md | the second pipeline for debug lines: `PipelineSettings`, `LineBuffers`, the line pass | 2026-10-23 |
| systems-and-phases.md | the system registry, the phases and the fixed step, and what they measured | 2026-10-26 |
| transform.md | one `Transform` with a quaternion, and why nine math types became trivially copyable | 2026-10-27 |
| physics-scene.md | the physics scene: colliders, their wires, the C and M toggles, and picking | 2026-10-30 |
| dangle-physics.md | a picked shape hanging from the mouse: the handout and slides versions, inertia, grip, and what each milestone found | 2026-11-03 |
| ecs-chunks.md | archetype rows in fixed-size chunks from a `ChunkPool`, `ForEachChunk`, and the 4, 16 and 64 KB measurements | 2026-11-05 |
| create-with-components.md | `CreateEntity(values...)` and `CreateEntityByComponentIds`: a spawn with zero migrations, in the World and the command buffer | 2026-11-05 |
| clear-the-world.md | `World::Clear`, `Archetype::ReleaseChunks`, and R in `PhysicsScene` | 2026-11-05 |
| intrusive-free-list.md | the free slot list kept in each dead slot's `row`, `CheckFreeList`, and why `totalRows` left `WorldStats` | 2026-11-05 |
| entity-breakpoints.md | the `DEBUG::DebugWatch` tag, the system filter, and the `[[unlikely]]` that took back a 45% slowdown | 2026-11-05 |
| frame-cap-and-timer.md | `Timer` in nanoseconds, `FrameCap` with sleep then spin, the wait options measured, and the NVIDIA power-mode finding | 2026-11-11 |
| present-mode-and-fullscreen.md | `PresentMode` as a setting with a FIFO fallback, Ctrl+P switching through the rebuild path, Alt+Enter borderless fullscreen | 2026-11-11 |
| live-fps-and-pacing-scene.md | `FrameTimeStats` and the once-a-second title and console line, the `WorldFrameStats` rename, the Pacing Scene and its load steps | 2026-11-11 |
| tools/ | the long original manuals of the Memory Monitor and the Profiler | 2026-10-20 |

## Plan steps

Other docs name the steps of a plan ("step 3", "4b"). These tables say where each one is written up.

**The frame pacing plan, ended 2026-11-11.** It came from ROADMAP.md, Rendering, "Frame pacing and
the present mode". Each note holds its steps' whole plan sections.

| step | feature | note |
|---|---|---|
| 1 | `Timer` counts nanoseconds | frame-cap-and-timer.md |
| 2 | the present mode as a setting | present-mode-and-fullscreen.md |
| 3 | Ctrl+P switches the mode at runtime | present-mode-and-fullscreen.md |
| 4 | Alt+Enter fullscreen | present-mode-and-fullscreen.md |
| 5 | the frame cap with a precise sleep | frame-cap-and-timer.md |
| 6 | live fps in the title and the console | live-fps-and-pacing-scene.md |
| 7 | the pacing scene | live-fps-and-pacing-scene.md |

**The ECS hardening plan, ended 2026-11-05.** It came from ROADMAP.md, ECS, Hardening, rows 1, 2,
4, 7 and 12. Each step's note holds its whole plan section.

| step | feature | note |
|---|---|---|
| 1 | chunks | ecs-chunks.md |
| 2 | create with components | create-with-components.md |
| 3 | clear the World | clear-the-world.md |
| 4 | an intrusive free list | intrusive-free-list.md |
| 5 | entity breakpoints | entity-breakpoints.md |

**The plan that ended on 2026-11-03.**

| step | feature | note |
|---|---|---|
| 1 | the scene switcher and a smaller `main` | scene-switcher.md |
| 1b | the fly camera | fly-camera.md |
| 2 | the line pipeline | line-pipeline.md |
| 3 | systems and phases | systems-and-phases.md |
| 4.0 | one `Transform` with a quaternion | transform.md |
| 4a | the physics scene, colliders and their wires | physics-scene.md |
| 4b | picking | physics-scene.md |
| 4c | the dangle | dangle-physics.md |
