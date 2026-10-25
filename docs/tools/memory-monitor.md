# MemoryMonitor

## The problem

Two memory mistakes are easy to make in C++ and hard to find: memory that is never given back (a
leak), and memory given back twice (a double free). Without a tool, a leak shows no sign until the
program runs out of memory, and a double free crashes far away from the line that caused it.

## Before and after

Before, the program gave no sign of either mistake. With the monitor, a leak shows up in a report
with the file and line that made it:

```text
-- Leak Report --
Leak:      | address = 0x1d4f2a0 | size = 4 bytes | Main.cpp:8
Leaked blocks       : 1
Leaked bytes        : 4 bytes
```

And a double free is refused instead of corrupting the heap:

```text
Refused:   free   | delete(void*, size_t)                | address = 0x... | bad check value: freed twice, or not from here
```

The engine prints the leak report every time it closes. Since the Vulkan renderer was finished,
it lists 0 blocks.

## How it works

The monitor replaces `new` and `delete` for the whole program, so every allocation passes through
it, including those made by `std::vector`, `std::string` and the C++ runtime. Each block gets a
small header in front of it. The header records the block's size and alignment. In Debug it also
holds a check value, a link into a list of every live block, and the file and line that made it.

- On `new`, the monitor counts the bytes, prints one coloured line, and adds the block to the list.
- On `delete`, it reads the header first. A wrong check value means the pointer was freed already,
  or never came from `new`, so it refuses to free it.
- At any point, `Report(baseline)` prints the counts since a moment you choose, and
  `ReportLeaks(baseline)` lists every block still alive since then.

Nothing needs setting up. Compile `src/utils/MemoryMonitor.cpp` into the program and it watches
from before `main` starts.

```cpp
const MemoryStats start = MemoryMonitor::GetStats();
int* kept = MEMMON_NEW int(42);       /* never deleted, on purpose */
MemoryMonitor::ReportLeaks(start);    /* lists kept, with its file and line */
```

## Files

- `include/utils/MemoryMonitor.h`, `src/utils/MemoryMonitor.cpp`: the whole tool. It needs nothing
  else, not even the math library, so these two files can be copied into another project.
- `tests/MemoryMonitorTests.cpp`: the checks. `demo/MemoryMonitorDemo.cpp`: every path it watches.
- `scripts/capture-output.sh`: saves the demo's output before and after a change, so a refactor of
  the monitor can be shown to change nothing.

## Decisions while building

- **The counters are ready before `main`.** They use `constinit`, so the runtime's own startup
  allocations are counted too. That is why a program measures from a baseline taken at the top of
  `main`, not from zero.
- **Printing never allocates.** The log runs inside `operator new`. If it allocated, it would call
  itself forever. Each line is built in a stack buffer and printed with `fputs`, never with
  `std::cout` or `std::format`.
- **`delete` trusts the header, not the caller.** The header stores the alignment, so the monitor
  always frees through the right function. A caller using the wrong `delete` gets a `Mismatch:`
  line, and the block is still freed correctly.
- **A refused free leaks the block on purpose.** Leaking one block is safer than freeing memory
  that may belong to something else.
- **`MEMMON_NEW` instead of `new` to get a file and line.** A plain `new` cannot know where it was
  called from. `MEMMON_MAKE_UNIQUE` does the same for `std::make_unique`.
- **One lock for all the bookkeeping.** Several threads can allocate at once. The lock keeps every
  count, every log line and the leak list whole. The heap itself still runs in parallel.
- **The test program is built without `/sdl`.** `/sdl` overwrites a deleted pointer with a junk
  address, and the double-free test deletes the same pointer twice on purpose.

## Questions a reader might ask

### Why not use a tool that already exists, such as Visual Studio's leak detection?

Those tools work, and they find more. Building this one was the course task: show where memory
goes and catch misuse, from inside the program. It also prints its counts while the game runs,
which fits the engine's other tools.

### Why are some checks Debug only?

The Debug header is 64 bytes on x64; the Release header is 16. The check value, the leak list and
the file and line all live in the bigger header. Release keeps the counts, the reports and the
`Mismatch:` check, and drops the rest to stay small.

### What does it not catch?

Writing past the end of a block, using a block after freeing it, mixing up `delete` and
`delete[]`, and memory taken with `malloc` or other calls that skip `new`. It also only builds on
Windows with MSVC, because it uses `_aligned_malloc` and a Windows lock.

### Does it slow the program down?

Every call takes the lock and prints a line while logging is on. Turn logging off with
`MemoryMonitor::SetLogging(false)` around busy code; the counts keep running. Measure performance
with logging off, or you measure the console.

## More detail

The long original, with every feature and limit and an example for each:
dev-notes/tools/memory-monitor.md. Build and test details: tools/build-and-test.md.
