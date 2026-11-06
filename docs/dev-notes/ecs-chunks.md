# Chunks: archetype rows in fixed-size chunks

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed 2026-11-04 (commit `c643e0d`) and built 2026-11-05 (commit `5de98168`), as step 1 of the
plan that ended on 2026-11-05, which hardened the ECS storage. It came from ROADMAP.md, ECS,
Hardening, row 1. "Step N" below means a step of that plan; README.md maps each one to its note.

Every archetype reserves `maxEntities` rows. Scene3 filled its 32 MB slab on wave 15, and 36 of its 41
archetypes were empty. The space prototype paid 3.65 MB for 4,064 reserved rows across 14
archetypes, 11 of them empty.

The roadmap row called these blocks. The plan uses chunk, the name Unity and Unreal Mass use.

A chunk is one fixed-size buffer that holds the same rows from every column of one archetype. The
archetype computes how many rows fit, once, from the chunk size and its row size. With an 8-byte
`Entity` and components of 12, 16 and 28 bytes, a row is 64 bytes, and a 16 KB chunk holds 256 rows:

| column | bytes per row | slice | offset |
|---|---|---|---|
| Entity | 8 | 256 × 8 = 2,048 | 0 |
| A | 12 | 256 × 12 = 3,072 | 2,048 |
| B | 16 | 256 × 16 = 4,096 | 5,120 |
| C | 28 | 256 × 28 = 7,168 | 9,216 |

Each offset is rounded up to its type's alignment, so a chunk can waste a few bytes at its end. An
archetype whose row is bigger than a chunk gets 0 rows and asserts.

## Decisions

1. **Chunks, not a growable column.** The World's arena never frees, so a growing vector leaves each
   old buffer behind, about twice the column's final size in dead bytes. Equal-size chunks can go back
   on a free list and any archetype can reuse them. The memory-manager step reserved rows for the same
   reason ([memory-manager.md](memory-manager.md), "Columns reserve `maxEntities` rows").
2. **Each chunk holds every column for a range of rows,** not each column cut into its own chunks. One
   row count fits the whole chunk, so a row is a chunk plus an offset, and a chunk empties as one
   piece. Unity lays its chunks out this way: one array per component plus one of entity IDs.
3. **`EntitySlot` keeps one row number.** The World derives the chunk with `row / rowsPerChunk` and
   `row % rowsPerChunk`. It works because every chunk is full except the last one: a remove swaps the
   archetype's last row, which sits in the last chunk, into the gap. `EntitySlot`, `Locate` and the
   swap-remove fix-up stay as they are. The cost is a divide per `GetComponent`, which is not in a
   hot loop.
4. **The World owns a `ChunkPool`.** It takes chunk-size pieces through the `ArenaResource`, the same
   way the World takes memory for an `Archetype`, so the arena's counters see the chunks. When its
   free list is empty it takes a new piece; when an archetype hands a chunk back it keeps it. The
   World builds the arena, the resource and the pool in that order. Archetypes hold a pointer to the
   pool and own nothing else.
5. **Systems loop with `ForEachChunk`.** The query walks every matching archetype and chunk and calls
   the system's lambda once per chunk. The lambda is a template parameter, not a `std::function`, so
   it can be inlined. The query owns the loop, so it can add work around each chunk later (a
   `DebugWatch` check, a profiler span, threads) without a system body changing. `return` in the
   lambda moves on to the next chunk; no system breaks out of its loop today.
6. **An archetype gives a chunk back the moment it empties.** A spare chunk per archetype would bring
   back memory held by empty archetypes, and step 2 removes the in-between hops that would churn.
   Unity frees a chunk when its last entity leaves. Only the last chunk ever empties, and adds and
   removes inside a chunk never touch the pool.
7. **`WorldConfig` keeps both fields.** `maxEntities` now sizes only `entitySlots` and `freeSlots`,
   because those vectors also live in the arena and must not grow. `arenaBytes` covers the slots, the
   archetypes, their layouts and the chunks in use at the peak: a real total a scene can measure.
   `arenaBytes = 0` still works: the resource sends every request to the heap, and the pool still
   reuses chunks.
8. **The chunk size is a `WorldConfig` field, `chunkBytes`, default 16 KB.** `EcsBench` can then build
   Worlds at 4, 16 and 64 KB in one run of one binary. Each archetype reads it once to compute
   `rowsPerChunk`.

## The shape

```cpp
/** Where one component type sits inside every chunk of an archetype. */
struct ColumnLayout
{
    ComponentId type;
    uint32_t elementSize;
    /** Bytes from the start of the chunk to this column's first element. */
    uint32_t offset;
};

/** One fixed-size buffer that holds the same rows of every column of one archetype. */
struct Chunk
{
    std::byte* bytes;
    uint32_t count;
};

class Archetype
{
private:
    ComponentMask mask;
    /** Filled once at creation. The same for every chunk. */
    std::pmr::vector<ColumnLayout> columns;
    /** Every chunk is full except the last one. */
    std::pmr::vector<Chunk> chunks;
    uint32_t rowsPerChunk;
    ChunkPool* pool;
};
```

A system loop, using `PickSystem`'s box colliders:

```cpp
boxColliderEntities.ForEachChunk([&](const ChunkView chunk)
{
    const Transform* transforms = chunk.Column<Transform>();
    const PHYSICS::BoxCollider* colliders = chunk.Column<PHYSICS::BoxCollider>();
    for (uint32_t row = 0; row < chunk.Count(); ++row)
    {
        /* ... */
        pickedEntity = chunk.Entities()[row];
    }
});
```

`ChunkView` holds a pointer to the archetype and one to the chunk. `Column<T>()` uses the archetype's
layout to find the column inside the chunk, and `Archetype()` gives the archetype itself.

## What to measure

- **The baseline to beat:** under a nanosecond per entity at 100,000 rows in `EcsBench`, already
  bandwidth bound. Chunks do not make the loop faster; the columns are already packed today.
- **Per chunk size** (4, 16 and 64 KB): nanoseconds per entity, the cost of one migration, and chunk
  bytes per live entity.
- **Three new `FrameStats` counters:** `chunksTaken`, `chunksReturned` and `chunksFree`. Decision 6 is
  wrong if a scene with a steady entity count shows chunks taken and returned frame after frame. Then
  the fix is one spare chunk per archetype. Scene3's waves are the test.
- The spawn cost belongs after step 2, because the in-between hops each take and return a chunk.

## Build notes

- `Archetype::Column<T>()` without a chunk goes away, so every system moves to `ForEachChunk` in the
  same change, or the build breaks. `World::GetComponent` and the migration code switch to the
  row-to-chunk lookup at the same time.
- Each archetype that holds rows costs at least one chunk: Scene3's 41 archetypes cost at most
  41 × 16 KB = 656 KB.
- **For later shared values:** a shared value should be part of an archetype's identity, so cell 7
  and cell 8 become two archetypes. Shared values inside one archetype would give each value its own
  part-full last chunk, and decision 3's divide would stop working.
- Power-of-two rows per chunk, a chunk size per archetype and sparse-set storage are in ROADMAP.md,
  Hardening.

## What the build changed

- **`ChunkView::Owner()`, not `Archetype()`.** A member function named `Archetype` would hide the
  class name inside `ChunkView`.
- **`Archetype::FindLayout` is public,** so `ChunkView::Column<T>()` can find a column's offset.
  `Chunks()` gives the chunk list to `ForEachChunk`.
- **The counters split by kind.** `chunksTaken` and `chunksReturned` count events, so they sit in
  `FrameStats`, and the pool writes them through a `FrameStats&` to the World's counters.
  `chunksFree` is a level, so it sits in `WorldStats`.
- **`World::maxRows`** replaces `reservedRows`. It only limits the slot table now.
- **The Scene3 check was dropped.** Scene3 lives on the prototype branch. Vakarian ran
  `PhysicsScene` instead, on 2026-11-05.

## What the build measured

`bin\x64\Release\EcsBench.exe`, against the same bench built from `7d265d5`. Each is the mean of 3
warm runs, with the cold run dropped. 100,000 entities, arena mode:

| | before | 4 KB | 16 KB | 64 KB |
|---|---|---|---|---|
| Movement, ns per entity | 0.70 | 0.91 | 0.78 | 0.73 |
| Playback, ms per frame | 0.420 | 0.429 | 0.433 | 0.430 |
| Spawn, ms | 17.1 | 20.6 | 20.3 | 20.4 |
| Arena high water | 10,548 KB | 4,780 KB | 4,759 KB | 4,895 KB |

- **Done when:** under 1 ns per entity holds at every size. Movement at 16 KB is about 11% slower,
  just above the 8% noise floor. A guess, not measured: the row loop starts again every 511 rows,
  and each chunk looks up its columns.
- **Memory:** the arena peak halves at 10,000 and 100,000 entities. At 1,000 entities a 64 KB chunk
  costs more than before (145 KB against 106 KB), because each archetype in use pays for one whole
  chunk. The bench's 193 KB slab then falls back to the heap for 2 chunks.
- **Decision 6:** chunks taken are about 2 per created entity (1,206,688 for 600,099 creates at
  100,000). Each spawn's hops through the empty and Position-only archetypes take a chunk and give it
  back. Step 2 removes those hops. Measure again there before adding a spare chunk per archetype.
