# Profiling

Moved from the semester plan on 2026-10-20.

---

## Profiling — done, and what it said

Built 2026-09-30. The course topic asks for two things: Visual Studio's diagnostics tools, and
your own timing with `std::chrono`. The second is where the work went, because it could be joined
to counters the ECS design had already been asking for.

### What got built

**`Profiler`** — `include/utils/Profiler.h`, `src/utils/Profiler.cpp`. Named spans with per-frame
min, average and max. `PROFILE_SCOPE("name")` times a block; `PROFILE_SCOPE_ECS(world, "name")`
times it and also records the structural changes it caused. One table, both numbers on one row.

**`FrameStats`** — `include/ecs/FrameStats.h`. Seven counters the `World` keeps for itself:
entities created and destroyed, components added and removed, migrations, bytes copied, archetypes
created. Plus `WorldStats`, a snapshot of what the world holds right now, and run totals.

**`Profiler::ReportFrame()`** — prints one frame instead of a summary. The trigger belongs to the
caller, which is why `Main.cpp` has `if (Profiler::LastFrameMilliseconds() > 30.0)`. Three lines
of automatic hitch capture.

**`EcsBench`** — `bench/EcsBench.cpp`, its own target, no SDL, builds on Win32 too. Three scales
for 1000 frames each, flat out. `Main.cpp` runs a thousand entities behind a 60 fps wait, which is
too small and too idle to measure.

### Four design decisions worth keeping written down

**Spans time, counters explain.** A clock says a block took 0.7 ms. It cannot say whether that was
work you asked for or bookkeeping the storage layout charged you. Movement touches every entity
and migrates none; a tag toggle touches a hundred and migrates all hundred. Both finish in under a
millisecond. Only the counters tell them apart, and only the pair tells you which will hurt when
it scales.

**`steady_clock`, not `Timer`.** `Timer` counts whole milliseconds, which is right for a frame
delta and useless for a span costing a fifth of one. `Timer` still drives the loop; the profiler
only measures.

**It allocates nothing.** Fixed table of 64 spans, printing formatted into a stack buffer through
`fputs` rather than `std::cout`. The Memory Monitor counts every `new` in the program, so a
profiler that allocated would appear in those numbers as if the engine had asked for it — and
the arena demo (dev-notes/memory-manager.md) depends on that count being honest.

**It stays on in Release.** `GE5_PROFILING` defaults to 1 in both configurations. Debug timings
describe the debugger: `OnCreate` measures 38.8 ms in Debug and 2.5 ms in Release, fifteen times
apart. Every number worth keeping comes from Release, so a profiler that compiled out there would
leave nothing to read.

### What it measured

`EcsBench`, Release, x64. Each frame moves every entity, kills one row in twenty every tenth
frame and respawns them, and toggles a tag on one row in ten every fifth frame.

Means of three runs. One run is not reproducible: timings move about 8% between runs, and the
first ever run of a fresh binary is roughly 25% slower than the mean. Counters do not move at all.

| entities | Movement avg | Playback avg | migrations/frame | worst Playback | worst frame |
|---|---|---|---|---|---|
| 1,000 | 0.001 ms | 0.004 ms | 37.9 | 0.065 ms | 0.118 ms |
| 10,000 | 0.007 ms | 0.043 ms | 376.4 | 0.457 ms | 0.491 ms |
| 100,000 | 0.074 ms | 0.465 ms | 3,762.9 | 3.835 ms | 4.159 ms |

**Archetype churn is affordable, and that is the headline.** A migration costs 0.114 to 0.124
microseconds, and the figure holds at all three scales, so churn scales linearly rather than
degrading. The worst frame at 100,000 entities moved 24,000 entities between archetypes and still
finished in 4.2 ms. It would take roughly 130,000 migrations in a single frame to miss 60 fps.

That closes the question that ECS.md's "Later — from the EnTT comparison" raised (now in archive/snapshot-2026-11-03/living/ECS.md) after the EnTT comparison. Adding a component
here copies a whole row where a per-component design would move nothing, and the measurement says
the row copy is cheap enough at every scale this engine will see. Enableable components stay
unbuilt.

**Iteration is under a nanosecond per entity** — 0.074 ms for 100,000 rows. Bandwidth bound
rather than instruction bound, which is the payoff for grouping by archetype and the reason the
storage decision in ECS.md, "Decisions that are settled", held.

**Deferring costs about 3% of the work it defers.** Recording 24,000 commands takes 0.12 ms;
applying them takes 3.8 ms. The command buffer earns its place.

**Entity slots are reused, not reallocated.** The 100,000 run created 600,099 entities and
destroyed 500,099; the slot table finished at exactly 100,000 — the peak alive and no more. The
pool behaviour claimed in dev-notes/memory-manager.md is real.

**Archetypes do not accumulate.** Four archetypes, two of them empty, at every scale, after 3.9
million migrations.

### Two things deliberately not done

**The Chrome trace file.** Cherno's video writes spans as Chrome trace JSON and opens the result
as a flame chart, and it would have made a good screenshot. It was skipped for two reasons: the
profiler keeps aggregates only, so a timeline needs a new fixed-size event buffer rather than a
40-line writer; and the VS2022 CPU session already draws a timeline for free.

**The VS2022 sampling session.** Skipped to close the topic. Worth knowing what it would have been
for: instrumented spans find *which block* is slow, sampling finds *which line inside it*.
`EcsBench` at 100,000 entities exists precisely so a sampler has enough load to say something, and
`kFrames` would need raising first — the whole benchmark finishes in about a second, which is too
few samples to trust.

### What it cost

Roughly 620 lines across `Profiler.h`, `Profiler.cpp` and `FrameStats.h`, against about 2,000
lines of engine outside the Memory Monitor. That is a lot for a topic worth 10%, and the ratio is
the reason a stopping rule was written mid-way: **build it when you can point at the moment you
wanted it, not when you can imagine one.** Applying that rule cut `Profiler::SetLogging` and the
whole of `CommandBufferStats` before this section was written.

---

