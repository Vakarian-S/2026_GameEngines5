# Profiler

## The problem

A clock can say a block of code took 0.7 ms. It cannot say why. In an ECS, two blocks can take the
same time for very different reasons: a movement loop touches every entity and moves none between
archetypes, while a system that adds a tag touches a hundred entities and copies all hundred to a
new archetype. Only the second one gets worse as the game grows. The engine needed a tool that
shows the time and the ECS work side by side.

## Before and after

Before, the only timing was the frame time, in whole milliseconds. Now each named block gets a row
with its time and the ECS work it caused:

```text
-- Profile Report --
  frames 1088
  frame ms   min 15.059   avg 16.348   max 22.008   (61.2 fps)
  span                    frames calls   min ms   avg ms   max ms     mig/f mig max   bytes/f
  Wait                      1088  1088   15.034   16.283   17.372       0.0       0         0
  Playback                  1088  1088    0.000    0.009    0.672       1.7     100        39
  Movement                  1088  1088    0.004    0.008    0.090       0.0       0         0
```

`mig` is migrations: entities moved from one archetype to another. Measured with it on
2026-09-30, `EcsBench` in x64 Release, mean of 3 runs: at 100,000 entities a migration costs about
0.12 microseconds, and the worst frame moved 24,000 entities in 4.2 ms. That answered whether the
archetype design could afford its copying. It can.

## How it works

Wrap a block in `PROFILE_SCOPE("Name")`. When the block starts, the profiler reads a precise clock;
when it ends, it reads the clock again and adds the difference to that name's row.
`PROFILE_SCOPE_ECS(world, "Name")` also reads the world's counters before and after, so the row
records the migrations and bytes copied that the block caused.

Rows are added up per frame, between `Profiler::BeginFrame()` and `Profiler::EndFrame()`. At exit,
`Profiler::Report()` prints the table, sorted with the most expensive block first.
`Profiler::ReportFrame()` prints only the last frame; `Main.cpp` calls it when a frame takes longer
than 30 ms, which catches hitches.

```cpp
Profiler::BeginFrame();
{
    PROFILE_SCOPE_ECS(world, "Playback");
    commandBuffer.Playback(world);
}
Profiler::EndFrame();
```

## Files

- `include/utils/Profiler.h`, `src/utils/Profiler.cpp`: the profiler and its macros.
- `include/ecs/FrameStats.h`: the counters the `World` keeps about its own changes.
- `bench/EcsBench.cpp`: a heavy load at 1,000, 10,000 and 100,000 entities, built to give the
  profiler something worth measuring.

## Decisions while building

- **The counters live in the ECS, not in the profiler.** `World` counts its own changes in
  `FrameStats`. The profiler only reads them before and after a block.
- **`std::chrono::steady_clock`, not the engine's `Timer`.** `Timer` counts whole milliseconds,
  fine for a frame and useless for a block that takes a fifth of one.
- **Names must be string literals.** The profiler compares names by address, not by text, which is
  fast. A name built at runtime gets a new address each frame and fills the table.
- **It never allocates.** The table is a fixed array of 64 rows, and printing uses a stack buffer.
  The MemoryMonitor counts every allocation, and a profiler that allocated would show up in those
  counts as if the engine had asked for the memory.
- **Numbers are per frame, not per call.** A block entered 500 times in one frame counts as one
  sample: its total for that frame. At 60 fps the question is what a block costs per frame.
- **It stays on in Release.** Debug timings describe the debugger: one startup block measured
  38.8 ms in Debug and 2.5 ms in Release. Set `GE5_PROFILING` to 0 to remove it completely.

## Questions a reader might ask

### Why not just use Visual Studio's profiler?

They answer different questions. This profiler finds which block is slow, and shows the ECS work
next to the time. Visual Studio's sampling profiler finds which line inside a block is slow. Use
both: this one to find the block, Visual Studio's (Debug, Performance Profiler, CPU Usage, on a
Release build) to find the line.

### Why can it not draw a timeline or a flame chart?

It keeps only totals, minimums and maximums for each row, not every single occurrence. A timeline
needs a buffer of every event, which was not built because Visual Studio already draws one.

### Can several threads use it?

No. It has one table and no lock, because the engine runs on one thread. The plan for threads is
one table per thread, merged at the end of the frame.

### What does it cost?

About 100 nanoseconds per block: two clock reads and a search of the table. Put it around a
system or a phase, never inside a loop over entities.

### Does it measure the GPU?

No. It reads a CPU clock. GPU time needs timestamp queries inside Vulkan, which are not built.

## More detail

The long original, with every output and limit: dev-notes/tools/profiler.md. Why it was built and
the full measurements: dev-notes/profiling.md.
