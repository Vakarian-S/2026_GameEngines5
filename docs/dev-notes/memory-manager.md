# Memory Manager

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Moved from the semester plan on 2026-10-20.

---

## Memory Manager — done

The course description asks to *"demonstrate that you can control the use of memory over just
using the OS."*

Built 2026-10-01. How it works, the rules for using it and every measurement are in README.md
under "Arena". This section keeps the decisions and the reasons behind them.

**What was already there.** Entity slots are a pool: fixed-size records, a free list of dead
indices, reuse rather than reallocation. Archetype columns have pool shape too — dense arrays,
no per-entity allocation, swap-remove instead of holes. Creating and destroying entities
allocates nothing. What was missing was control of where the memory came from: every column
growth called `operator new`.

**What was built.**

- `Arena` and `ArenaResource`, so a `std::pmr` container can allocate from an arena.
- `World(WorldConfig)`, which takes one slab. Slots, free list, archetype objects and every
  column live inside it.
- A frame arena in `Main.cpp`, reset once per frame, behind the draw list.
- `ArenaDemo`, which runs the same work on the heap and from an arena and prints both counts.
- `EcsBench`, which runs each scale in both modes.

### Decisions

| decision | reason |
|---|---|
| **Columns reserve `maxEntities` rows once and never grow** | A growing vector abandons its old block, and a bump arena cannot reuse it. A column growing from empty leaves dead bytes of about twice its final size, and the World's arena never resets |
| **`std::pmr::vector` for columns, slots and lists** | `ArenaResource` already existed and was tested. A hand-written buffer saves nothing |
| **Archetype objects live in the arena too** | Built with placement new and destroyed by hand. This is what brings a World to exactly one call to `new` |
| **The switch is a size, not a macro** | `arenaBytes = 0` turns the arena off, and one program can compare both modes. `GE5_USE_ARENA` could only compare two builds, so it was removed |
| **`CommandBuffer`, `Query` and the scene's vectors stay on the heap** | They keep their blocks from frame to frame. A frame reset would leave them pointing at reused memory |
| **`DrawList` is a `pmr::vector` and the caller picks its memory** | That is what lets the frame arena stand behind it |
| **No level arena in code** | No level exists to unload. It stays in the writeup as the third lifetime |
| **No garbage collector** | Games avoid GC because unpredictable pauses ruin frame timing. `Entity {index, version}` is the alternative: a safe reference to something that may be gone, with no ownership and no collector |

The three lifetimes:

```
permanent   taken once at startup, never freed     the ECS
level       reset when a level unloads             not built
frame       reset every frame                      scratch, the draw list
```

### What it measured

x64 Release. Details and the full bench table are in README.md.

- A World of 1000 entities makes 1 call to `new` where the heap version makes 74.
- A frame's scratch list makes 0 calls where the heap version makes 18.
- Spawning 100,000 entities takes 18.2 ms against 22.1 ms, because the columns no longer
  reallocate. Means of three runs.
- Per-frame cost does not change, and the worst frame is no smoother. The per-frame path never
  allocated.

**The arena is a control feature, not a speed feature.** At 1000 entities the heap handles all of
this without trouble. The value is one known footprint, per-arena numbers, and a scratch region
that frees in one step. It matters on a console with a fixed budget, in a session long enough to
fragment the heap, or where a rare slow allocation would show as a hitch. None of those is
reproduced at this scale.

### What it cost

- Every archetype reserves the full maximum. The bench load uses 108 bytes per entity across four
  archetypes: 10.8 MB at 100,000 entities, with two of the four nearly empty. Chunks are the real
  fix and stay out of scope.
- Every arena is sized by hand. A full arena asserts in Debug and falls back to the heap in
  Release.
- New rules: pass the resource to every `pmr` container, build frame containers after the reset,
  never copy a `pmr` container.

### Four things that broke a staged build

The code handles each one. README.md has the detail.

1. A byte vector asks for alignment 1, and a bump arena honors that exactly. Columns would sit on
   odd addresses.
2. `std::pmr::new_delete_resource()` allocates inside the runtime, where the Memory Monitor cannot
   count it.
3. MSVC Debug containers allocate when constructed, so Debug shows more arena calls than Release.
4. A copied `pmr` vector lands on the default heap.

### Two claims earlier versions made that were false

An early version said the arena fixes columns reallocating their way up from empty. It does not.
It makes each reallocation cheap and makes the waste permanent. Reserving up front is what fixes
it.

A first measurement also suggested the Memory Monitor taxed allocation about ninefold, which
would have meant the arena removed that tax too. Measured properly on 2026-09-30, with the same
1000-entity spawn with and without the monitor compiled in, three runs each, there is no
measurable difference. The earlier figure came from a run with `MemoryMonitor::SetLogging(true)`
around the spawn, which prints a colored line per allocation. That is console I/O, not allocator
cost. **Leave logging on and you measure the console.**

---

