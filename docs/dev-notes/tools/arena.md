Moved unchanged from README.md on 2026-10-20. This is the long original. The short version for reading is in docs/tools.

# Arena

Memory you take once and hand out yourself. One block comes from the OS. A pointer marks where
the free space starts. Allocating moves the pointer forward, and freeing everything moves it back.

```cpp
#include "ecs/World.h"
#include "utils/Arena.h"
#include "utils/ArenaResource.h"

/* The World takes its own slab. arenaBytes = 0 means no arena, and it allocates from the heap. */
WorldConfig config;
config.maxEntities = 1000;
config.arenaBytes = 256 * 1024;
World world(config);

/* A scratch arena for one frame, owned by the loop. */
Arena frameArena(64 * 1024);
ArenaResource frameMemory(frameArena);

while (running)
{
    /* Build the list after the reset. It is gone at the end of the iteration, before the next one. */
    frameArena.Reset();
    DrawList drawList(&frameMemory);
    scene.MakeDrawList(drawList);
}
```

The files are `include/utils/Arena.h`, `include/utils/ArenaResource.h` and their `.cpp` files
under `src/utils/`. `World` and `Archetype` use them. `ArenaDemo` runs the same work on the heap
and from an arena and prints both counts.

### What it does

#### 1. Hands out slices of one block

`Arena` takes one block with `operator new` and keeps three pointers: the start, the next free
byte, and the end. `Allocate(size, alignment)` rounds the next free byte up to the alignment,
returns that address, and moves the pointer forward by `size`. The bytes skipped by the rounding
are counted as padding. If the rest of the block is too small, it returns `nullptr` and counts a
refusal.

#### 2. Frees everything at once, and nothing else

`Reset()` moves the pointer back to the start. There is no way to free one slice, because the arena
keeps no list of what it handed out.

Every pointer taken from the arena is stale after a reset, and nothing tells you. Debug fills the
bytes that were handed out with `0xDD` first, so a stale read returns an obvious value. Release
does not. A reset runs no destructors, so only put plain data in an arena.

#### 3. Lets a standard container allocate from it

`ArenaResource` is a `std::pmr::memory_resource`. A `std::pmr::vector` takes a pointer to one when
you construct it and asks it for every block. The vector code is the same as `std::vector`. Only
the source of memory changes, and it is chosen at runtime, which is how one `World` runs with or
without an arena.

A `pmr` container does not find the resource on its own. Every container in `World` and `Archetype`
gets the pointer by hand, and a forgotten one falls back to the heap without an error.

#### 4. Gives the World one slab

`World(WorldConfig)` takes a maximum entity count and a slab size. The entity slots, the free list,
the archetype list, each `Archetype` object, each entity list and every component column allocate
from that one slab.

Each archetype reserves `maxEntities` rows per column when it is created, so a column never grows.
That is deliberate. A vector that runs out of room takes a bigger block and abandons the old one,
and an arena cannot reuse the old block. A column growing from empty leaves dead bytes of about
twice its final size, and the World's arena never resets.

`arenaBytes = 0` turns the arena off. The World then allocates from the heap and grows as it did
before the arena existed.

#### 5. Gives each frame a scratch arena

`Main.cpp` owns a second `Arena`. It resets at the top of each frame, and the draw list is built
from it after the reset. At 1000 entities the draw list takes 12,000 bytes a frame, and the arena
never touches the heap.

The draw list is the only user today. `Scene0::Update` has no scratch data yet. Put a list there
when a system needs one, not before.

#### 6. Falls back to the heap instead of failing

When the arena is full, `ArenaResource` serves the request with plain `new` and counts it. Debug
asserts first, so the size gets fixed instead of ignored.

A World built with `arenaBytes = 0` has an arena with no slab behind it, so every call takes this
path. That is how the heap mode in `ArenaDemo` and `EcsBench` runs, and it goes through the
program's own `operator new`, so `MemoryMonitor` still counts it.

#### 7. Reports its own numbers

`ArenaStats` holds the capacity, bytes used, the high water mark, padding, allocations, refusals
and resets. `ArenaResourceStats` holds how many calls the arena served and how many fell to the
heap. `World::MemoryStats()` returns both for the World's arena.

Size an arena from its `highWater`. `Main.cpp` prints both arenas' numbers at exit.

### Rules when you use it

- Pass the resource to every `pmr` container by hand.
- Build a frame container inside the loop body, after the reset. Scope then ends its life before
  the next reset.
- Never store a frame resource in a class member. Take it as a function argument.
- Do not copy a `pmr` container. The copy lands on the default heap resource, so
  `ComponentColumn` can only be moved.
- Keep plain data in an arena. Nothing runs destructors on a reset.
- Declare the `Arena` and its `ArenaResource` before every container that uses them, as `World`
  does. Members are destroyed in reverse order, and a container that outlives its resource gives
  its memory back to a dead object.

### What it measured

x64 Release. `ArenaDemo` counts every call to `new` with the `MemoryMonitor`.

```
-- a world of 1000 entities, Position and Velocity --
  heap    74 calls to new, 157598 bytes
  arena   1 calls to new, 262144 bytes
    used 76880 bytes, padding 8 bytes, 14 allocations, 0 fell to heap

-- 5 frames, each filling a list of 1000 positions --
  heap    90 calls to new, 192765 bytes
  arena   0 calls to new, 0 bytes
    high water 38552 bytes, 5 resets, 0 fell to heap
```

The World goes from 74 calls to 1. A frame's scratch list goes from 18 calls to 0.

The demo grows its list with `push_back`, so the frame arena's high water is 38,552 bytes for
12,000 bytes of data. That is the growth waste from point 4 in miniature. The engine's draw list
reserves once, and its frame arena peaks at 12,000 bytes.

The engine makes 2 calls to `new` while its scene starts: the 262,144 byte slab, and the scene's
own entity list, 1000 handles of 8 bytes that the monitor logs as 8039 bytes. A bare
`std::vector<Entity>` reserving 1000 logs the same number. The World used 109,040 of its slab's
bytes, and nothing fell to the heap.

`EcsBench` runs each scale on the heap and on the arena. Means of three runs, 100,000 entities:

| span | heap | arena |
|---|---|---|
| Spawn | 22.1 ms | 18.2 ms |
| Playback avg | 0.433 ms | 0.429 ms |
| Movement avg | 0.068 ms | 0.070 ms |

**Spawning is faster, and nothing else is.** The columns no longer reallocate while 100,000
entities arrive. The per-frame numbers sit inside the roughly 8% noise between runs. The worst
Playback frame ran from 2.8 to 4.4 ms in both modes, so the arena did not smooth the worst frame at
this scale.

**The cost is memory.** The bench load uses 108 bytes per entity across its four archetypes, which
is 10.8 MB at 100,000 entities. Two of those archetypes hold almost no rows.

At 1000 entities the heap handles all of this without trouble. The arena gives control: one known
footprint, per-arena numbers, and a scratch region that frees in one step. It does not give speed
at this scale.

### What went wrong while building it

Four things broke a staged build. The code now handles each one.

1. **A byte vector asks for alignment 1.** The heap returns 16 byte aligned memory anyway, so
   `std::vector<std::byte>` columns held floats safely. A bump arena honors the request exactly and
   would put a `Position` column on an odd address. `ArenaResource` raises every request to the
   default `new` alignment, and `Register<T>` rejects a component that needs more.
2. **`std::pmr::new_delete_resource()` is invisible to the monitor.** It allocates inside the runtime
   DLL, which does not call the program's `operator new`. The first demo run printed 0 calls on the
   heap. Heap mode now uses an `ArenaResource` over an arena with no capacity.
3. **MSVC Debug containers allocate when you construct them.** Each takes a small block for
   iterator checking. Debug shows 35 arena allocations where Release shows 19.
4. **A copied `pmr` vector lands on the default heap.** `ComponentColumn` has a deleted copy and a
   defaulted move.

### What it does not do

#### 1. It does not free one block

Only `Reset` frees, and it frees everything. Anything that needs its own lifetime stays on the
heap.

#### 2. It does not run destructors

A reset moves a pointer and nothing more. A type that owns a handle, a file or a Vulkan object does
not belong in an arena.

#### 3. It does not catch stale pointers in Release

Debug fills a reset arena with `0xDD`, which makes a stale read obvious. A stale write still
corrupts the next owner, in both builds. Only a sanitizer would catch that. MSVC's sanitizer likely
needs the Debug info format in `CommonSettings.cmake` changed first. That has not been tried.

#### 4. It reserves the maximum for every archetype

A new archetype costs `maxEntities` rows per column, even if it holds ten. Chunks would fix this,
and they are out of scope.

#### 5. You size it by hand

A full arena asserts in Debug. In Release it falls back to the heap and counts it in `fromHeap`. A
full archetype or slot table asserts in Debug, and in Release its vector grows past the reserve and
moves, so a cached column pointer goes stale. Read `highWater` and size from that.

#### 6. It is not thread safe

One arena, no lock. Give each thread its own when concurrency arrives.

#### 7. It does not cover everything

`CommandBuffer`, `Query`, and the scene's own vectors stay on the heap. Their vectors keep their
blocks from frame to frame, and a frame reset would leave them pointing at reused memory.

#### 8. It does not allow over-aligned components

A component can need at most the default `new` alignment: 16 on x64, 8 on Win32. A type with
`alignas(16)` does not compile as a component on Win32.

#### 9. It does not manage GPU memory

Vulkan allocates device memory itself. The arena and the renderer never touch each other.

#### 10. It is not a level arena

The plan names three lifetimes: permanent, level and frame. Only the first and the last exist in
code. No level exists to unload.

