> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Moved unchanged from README.md on 2026-10-20. This is the long original. The short version for reading is in docs/tools.

# Profiler

Two questions about a block of code: how long it took, and how much structural work it made the
ECS do. One table answers both.

```cpp
#include "utils/Profiler.h"

while (running)
{
    Profiler::BeginFrame();
    world.ResetFrameStats();

    {
        PROFILE_SCOPE("Movement");
        MoveEverything();
    }
    {
        PROFILE_SCOPE_ECS(world, "Playback");
        commandBuffer.Playback(world);
    }

    Profiler::EndFrame();

    /* Over budget, so dump the frame while its numbers are still in the table. */
    if (Profiler::LastFrameMilliseconds() > 30.0) Profiler::ReportFrame();
}

Profiler::Report();
```

The counters come from the ECS, not from the profiler. `World` counts its own structural changes
in `FrameStats`, `PROFILE_SCOPE_ECS` reads them before and after the block, and the difference is
what the block caused. `include/ecs/FrameStats.h` holds the struct and the `GE5_ECS_STATS` switch.

### What it does

#### 1. Times a named block

`PROFILE_SCOPE("name")` starts a `steady_clock` reading and records the difference when the block
ends. `PROFILE_FUNCTION()` does the same and takes the name from `__FUNCTION__`.

Names are compared by pointer, never by content, so a name has to be a string literal or
`__FUNCTION__`. A name built at runtime gets a new address every frame and fills the table, which
is why the report says how many records it had to drop.

`Timer` is not used for this. It counts whole milliseconds, which is right for a frame delta and
useless for a span that takes a fifth of one.

#### 2. Counts the structural work a block caused

`PROFILE_SCOPE_ECS(world, "name")` does everything `PROFILE_SCOPE` does, and also reads the
world's counters on the way in and on the way out. The row then carries migrations and bytes
copied alongside the timings.

This separates work from bookkeeping. A movement loop touches every entity and migrates none —
all work. A system that toggles a tag touches a hundred entities and migrates all hundred — all
bookkeeping. Both take under a millisecond, so the clock alone cannot tell them apart.

#### 3. Reports per frame, not per call

A span entered five hundred times in one frame is one sample: its total for that frame. `min`,
`avg` and `max` are across frames.

That is the unit that matters. At 60 fps you have 16.6 ms, and the question is what a block costs
you per frame, not per visit. `calls` is still reported, so the per-visit figure is a division
away.

A block timed outside `BeginFrame`/`EndFrame` still lands in the table, counted as its own frame.
Startup work shows up without a loop wrapped around it.

#### 4. Dumps one frame when you ask

`ReportFrame()` prints what every span did in the frame that just closed, and nothing else.
`BeginFrame` is what clears the per-frame numbers, so they survive until the next frame starts.

The trigger is yours. `LastFrameMilliseconds()` makes automatic hitch capture three lines, and a
key press or `if (frameNumber == 347)` works the same way. The tool does not guess when you care.

#### 5. Never allocates

The table is a fixed array of 64 spans. Printing formats into a stack buffer and calls `fputs`,
because `std::cout` allocates on first use.

That is not a detail. `MemoryMonitor` counts every `new` in the program, and a profiler that
allocated would show up in those numbers as if the engine had asked for it.

#### 6. Compiles out

`GE5_PROFILING` defaults to 1 in both Debug and Release. Set it to 0 and the macros expand to
nothing.

It stays on in Release on purpose. Debug timings describe the debugger, not a shipped build, so
every number worth keeping comes from Release — and a profiler that vanished there would leave
nothing to read.

`GE5_ECS_STATS` does the same for the counters, though the case for switching those off is much
weaker: a counter is one integer add next to work that already costs more.

### The three outputs

**One frame, during the run.** Printed only when your condition fires.

```
-- Frame 240 --  136.059 ms
  span                     calls        ms     mig     bytes
  Events                       1   120.000       0         0
  Movement                     1     0.011       0         0
  Playback                     1     0.043     100      2400
  MakeDrawList                 1     0.000       0         0
  Wait                         1    16.000       0         0
  Damage                       1     0.001       0         0
```

**The span table, at exit.** Sorted by total time, so the expensive block is first.

```
-- Profile Report --
  frames 1088
  frame ms   min 15.059   avg 16.348   max 22.008   (61.2 fps)
  span                    frames calls   min ms   avg ms   max ms     mig/f mig max   bytes/f
  Wait                      1088  1088   15.034   16.283   17.372       0.0       0         0
  Events                    1088  1089    0.003    0.042    5.794       0.0       0         0
  OnCreate                     1     1   38.849   38.849   38.849    2000.0    2000     12000
  Playback                  1088  1088    0.000    0.009    0.672       1.7     100        39
  Movement                  1088  1088    0.004    0.008    0.090       0.0       0         0
```

**Run totals, at exit.** `World::Totals()` and `World::Snapshot()`, printed by `Main.cpp`.

```
-- ECS Report --
  entities created    1000
  migrations          3800
  bytes copied        55200
  live entities       1000
  archetypes now      4 (3 empty)
```

### What it measured at 100,000 entities

`EcsBench` runs three scales for 1000 frames each, flat out, with no frame cap. Each frame moves
every entity, kills one row in twenty every tenth frame and respawns them, and toggles a tag on
one row in ten every fifth frame. Release, x64.

Means of three runs. Timings move about 8% between runs and a fresh binary's first run is roughly
25% slower than the mean, so one run is not a measurement. The counters are identical every run.

| entities | Movement avg | Playback avg | migrations/frame | worst Playback | worst frame |
|---|---|---|---|---|---|
| 1,000 | 0.001 ms | 0.004 ms | 37.9 | 0.065 ms | 0.118 ms |
| 10,000 | 0.007 ms | 0.043 ms | 376.4 | 0.457 ms | 0.491 ms |
| 100,000 | 0.074 ms | 0.465 ms | 3,762.9 | 3.835 ms | 4.159 ms |

**Migration costs 0.114 to 0.124 microseconds, and that figure holds at all three scales.** Churn
scales linearly. It does not degrade as the world grows.

**The worst frame at 100,000 entities moved 24,000 entities between archetypes and still finished
in 4.2 ms**, inside a 16.6 ms budget. Extrapolating at 0.12 µs, it would take roughly 130,000
migrations in one frame to miss 60 fps.

That answers the question that ECS.md's "Later — from the EnTT comparison" raised (now in archive/snapshot-2026-11-03/living/ECS.md) about archetype churn. Adding a tag here
copies a whole row, where a per-component design like EnTT would push into one pool and move
nothing. The measurement says the row copy is affordable at every scale this engine will see, so
the archetype layout keeps its iteration speed without paying for it elsewhere.

**Iteration is under 1 nanosecond per entity.** 0.074 ms for 100,000 rows of `Position` and
`Velocity`. That is a straight walk through packed memory, bounded by bandwidth rather than by
instructions, and it is the whole reason for grouping entities by archetype.

**Deferring costs about 3% of the work it defers.** Recording 24,000 commands takes 0.12 ms.
Applying them takes 3.8 ms. So the command buffer's own overhead is small next to the migrations
it batches, which is what justifies recording changes instead of applying them in place.

**Entity slots are reused rather than reallocated.** The 100,000 run created 600,099 entities and
destroyed 500,099, and the slot table finished holding exactly 100,000 slots — the peak number
alive, and no more.

**Archetypes do not accumulate.** Four archetypes, two of them empty, at every scale, after 3.9
million migrations. Archetype count stays flat.

**One caveat worth knowing, and it is not the one you would guess.** `EcsBench` does not compile
`MemoryMonitor` and `GameEngines5` does, so the monitor looked like it was taxing allocation about
ninefold. It is not. The same 1000-entity spawn, three runs each with and without the monitor
compiled in, shows no measurable difference — with roughly 130 allocations in the whole program, a
header write and a lock cost nothing.

The engine's `OnCreate` row reads 44 ms because `Main.cpp` wraps it in
`MemoryMonitor::SetLogging(true)`, which prints a colored line per allocation. **That measures the
console, not the engine.** Turn logging off around anything you intend to time.

### What it does not do

#### 1. It does not find the slow line

A span tells you which block costs the time. It cannot tell you which line inside it. That is
what a sampling profiler does, and Visual Studio ships one: **Debug → Performance Profiler**, CPU
Usage, pointed at `EcsBench` in Release.

The two tools answer different questions and neither replaces the other. Instrumented timing
finds the room; sampling finds the line.

#### 2. It is not thread safe

One table, no locks. The engine is single threaded by design. One table per thread merged at the
sync point is the shape to use later, the same shape the command buffer already uses for
structural changes.

#### 3. It does not attribute counters to a span unless you ask

`PROFILE_SCOPE_ECS` needs a world handed to it, because the clock is global and the counters are
not. A program can hold several worlds, so the scope has to be told which one to read.

Plain `PROFILE_SCOPE` reports zero migrations whether or not the block caused any.

#### 4. It holds 64 spans

Names past the 64th are dropped, and the report says how many records it lost rather than staying
quiet about it.

#### 5. It keeps no history of individual spans

Only aggregates: total, min, max, and the worst frame. There is no list of every span occurrence,
so there is nothing to write out as a timeline for a flame chart viewer without adding an event
buffer first.

#### 6. It does not measure the GPU

There is no renderer yet. GPU work needs timestamp queries inside the graphics API, which is a
separate mechanism from a CPU clock.

#### 7. It is not free

Two clock reads and a scan of the table per span, roughly 100 nanoseconds. `PROFILE_SCOPE_ECS`
also copies the counter struct twice.

Put one around a system or a phase. A timer inside a per-entity loop measures the timer.

