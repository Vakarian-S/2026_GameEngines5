# Arena

## The problem

Every time a container grew, the program asked the operating system's heap for memory. A World of
1,000 entities made 74 separate requests. Nobody could say how much memory a scene used, and the
course asks to show control over memory instead of leaving it all to the OS.

## Before and after

Measured on 2026-10-01, x64 Release, with `ArenaDemo` counting every call to `new`:

| work | heap | arena |
|---|---|---|
| build a World of 1,000 entities | 74 calls | 1 call |
| fill a scratch list for one frame | 18 calls | 0 calls |
| spawn 100,000 entities (`EcsBench`, mean of 3 runs) | 22.1 ms | 18.2 ms |

The time for each frame did not change. The arena gives control, not speed, at this scale.

## How it works

The program takes one big block of memory at the start. A marker points at the first free byte.
Asking for memory hands out the bytes at the marker and moves the marker forward. Freeing works
only all at once: the marker jumps back to the start.

The engine uses two of them:

- **The World's arena** holds every entity slot and every component column. It is never reset.
- **The frame arena** in `Main.cpp` holds the draw list. It is reset at the top of every frame.

## Files

- `include/utils/Arena.h`, `src/utils/Arena.cpp`: the block and its marker.
- `include/utils/ArenaResource.h`, `src/utils/ArenaResource.cpp`: lets standard containers use an
  arena.
- `World` and `Archetype` take their memory from one.
- `demo/ArenaDemo.cpp` compares heap and arena. `tests/ArenaTests.cpp` checks it.

## Decisions while building

- **`std::pmr::vector` instead of `std::vector`.** A `pmr` container asks a "memory resource" for
  its blocks, and the resource is picked when the container is built. That is how the same `World`
  code runs with or without an arena. The resource must be passed to every container by hand; a
  forgotten one quietly uses the heap.
- **Columns reserve `maxEntities` rows up front.** A vector that grows takes a new, bigger block and
  leaves the old one behind. An arena cannot reuse the old block, so a growing column would waste
  about twice its final size.
- **`ArenaResource` raises every request to the default `new` alignment** (16 bytes on x64, 8 on
  Win32). A byte vector asks for alignment 1,
  and an arena honours that exactly. Without the raise, a `Position` column could start at an odd
  address.
- **`ComponentColumn` cannot be copied.** A copied `pmr` vector moves onto the default heap, so only
  moving is allowed.
- **When the arena is full, it falls back to the heap.** Debug asserts first, so the size gets
  fixed. Release counts it in `fromHeap` and keeps running.
- **Debug fills a reset arena with `0xDD`.** A pointer kept past a reset then reads an obvious
  value instead of plausible garbage.

## Questions a reader might ask

### Why not a general allocator that can free one object at a time?

Nothing that uses these arenas frees one object at a time. The World keeps its memory until the
scene ends, and the frame arena drops everything at once. Freeing everything costs one assignment,
and a general allocator would need bookkeeping for each object.

### Why is there no on/off flag, such as a boolean or a build setting, for the arena?

The size does that job. `arenaBytes = 0` means "no arena", and the World then uses the heap. A
build setting would need two separate builds to compare heap and arena. With the size, one
program can run both modes side by side, which is how `ArenaDemo` and `EcsBench` compare them.

### Why is there no arena for a whole level?

The design names three lifetimes: permanent, level and frame. Only permanent and frame exist in
code, because the engine has no level to unload yet.

### Why does the GPU not use it?

Vulkan allocates GPU memory through its own calls. The arena only manages CPU memory.

## More detail

The long original, with every measurement and edge case: dev-notes/tools/arena.md. The reasons
behind the design: dev-notes/memory-manager.md.
