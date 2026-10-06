# GameEngines5

A small C++ game engine project, built step by step. Right now it holds three finished pieces:
`MemoryMonitor`, which watches every piece of memory the program takes and gives back,
`Profiler`, which measures how long a block of code took and how much work it made the ECS do,
and `Arena`, which lets the World and the frame loop take their memory in one block instead of
asking the heap again and again.

## Contents

- [Layout](#layout)
  - [Building](#building)
  - [Opening it in an IDE](#opening-it-in-an-ide)
  - [Taking MemoryMonitor into another project](#taking-memorymonitor-into-another-project)
  - [The vendored libraries](#the-vendored-libraries)
- [MemoryMonitor](#memorymonitor)
  - [What it does](#what-it-does)
    - [1. Catches every `new` and `delete`](#1-catches-every-new-and-delete)
    - [2. Counts allocations from before `main`](#2-counts-allocations-from-before-main)
    - [3. Gives you a snapshot of the counters](#3-gives-you-a-snapshot-of-the-counters)
    - [4. Prints a report since a point you choose](#4-prints-a-report-since-a-point-you-choose)
    - [5. Logs every call, in color](#5-logs-every-call-in-color)
    - [6. Lets you turn the log off](#6-lets-you-turn-the-log-off)
    - [7. Handles over-aligned types](#7-handles-over-aligned-types)
    - [8. Frees through the right path, even when the caller is wrong](#8-frees-through-the-right-path-even-when-the-caller-is-wrong)
    - [9. Catches double frees and bad pointers (Debug only)](#9-catches-double-frees-and-bad-pointers-debug-only)
    - [10. Catches small writes just before a block (Debug only)](#10-catches-small-writes-just-before-a-block-debug-only)
    - [11. Tells you the cost of the header](#11-tells-you-the-cost-of-the-header)
    - [12. Never allocates while it logs](#12-never-allocates-while-it-logs)
    - [13. Lists the blocks that leaked (Debug only)](#13-lists-the-blocks-that-leaked-debug-only)
    - [14. Names the file and line of a leak, with `MEMMON_NEW` (Debug only)](#14-names-the-file-and-line-of-a-leak-with-memmon_new-debug-only)
    - [15. Tells you where a block came from (Debug only)](#15-tells-you-where-a-block-came-from-debug-only)
    - [16. Stops safely at a damaged block (Debug only)](#16-stops-safely-at-a-damaged-block-debug-only)
    - [17. Works when several threads allocate at once](#17-works-when-several-threads-allocate-at-once)
  - [What it does not do](#what-it-does-not-do)
    - [1. It does not replace the `nothrow` overloads](#1-it-does-not-replace-the-nothrow-overloads)
    - [2. It does not see memory that skips `operator new`](#2-it-does-not-see-memory-that-skips-operator-new)
    - [3. It does not name the file and line of every leak](#3-it-does-not-name-the-file-and-line-of-every-leak)
    - [4. It makes threads take turns](#4-it-makes-threads-take-turns)
    - [5. It does not check anything in Release, or list leaks there](#5-it-does-not-check-anything-in-release-or-list-leaks-there)
    - [6. It does not always name a double free as a double free](#6-it-does-not-always-name-a-double-free-as-a-double-free)
    - [7. It does not survive a truly wild pointer](#7-it-does-not-survive-a-truly-wild-pointer)
    - [8. It does not catch overruns past the end](#8-it-does-not-catch-overruns-past-the-end)
    - [9. It does not catch `delete` vs `delete[]` mixups, or a wrong size](#9-it-does-not-catch-delete-vs-delete-mixups-or-a-wrong-size)
    - [10. It does not catch use after free](#10-it-does-not-catch-use-after-free)
    - [11. It does not count the header in the byte totals](#11-it-does-not-count-the-header-in-the-byte-totals)
    - [12. It does not scope the peak and largest block](#12-it-does-not-scope-the-peak-and-largest-block)
    - [13. It does not print long lines in full, or print to a file](#13-it-does-not-print-long-lines-in-full-or-print-to-a-file)
    - [14. It does not build outside Windows and MSVC](#14-it-does-not-build-outside-windows-and-msvc)
    - [15. It does not list blocks behind a damaged one](#15-it-does-not-list-blocks-behind-a-damaged-one)
    - [16. It does not keep your own printing out of its lines](#16-it-does-not-keep-your-own-printing-out-of-its-lines)
    - [17. It is not free](#17-it-is-not-free)
- [Profiler](#profiler)
  - [What it does](#what-it-does-1)
    - [1. Times a named block](#1-times-a-named-block)
    - [2. Counts the structural work a block caused](#2-counts-the-structural-work-a-block-caused)
    - [3. Reports per frame, not per call](#3-reports-per-frame-not-per-call)
    - [4. Dumps one frame when you ask](#4-dumps-one-frame-when-you-ask)
    - [5. Never allocates](#5-never-allocates)
    - [6. Compiles out](#6-compiles-out)
  - [The three outputs](#the-three-outputs)
  - [What it measured at 100,000 entities](#what-it-measured-at-100000-entities)
  - [What it does not do](#what-it-does-not-do-1)
- [Arena](#arena)
  - [What it does](#what-it-does-2)
    - [1. Hands out slices of one block](#1-hands-out-slices-of-one-block)
    - [2. Frees everything at once, and nothing else](#2-frees-everything-at-once-and-nothing-else)
    - [3. Lets a standard container allocate from it](#3-lets-a-standard-container-allocate-from-it)
    - [4. Gives the World one slab](#4-gives-the-world-one-slab)
    - [5. Gives each frame a scratch arena](#5-gives-each-frame-a-scratch-arena)
    - [6. Falls back to the heap instead of failing](#6-falls-back-to-the-heap-instead-of-failing)
    - [7. Reports its own numbers](#7-reports-its-own-numbers)
  - [Rules when you use it](#rules-when-you-use-it)
  - [What it measured](#what-it-measured)
  - [What went wrong while building it](#what-went-wrong-while-building-it)
  - [What it does not do](#what-it-does-not-do-2)
- [Running the tests](#running-the-tests)
- [Checking that a refactor changed nothing](#checking-that-a-refactor-changed-nothing)

## Layout

Seven programs and one library.

| Target | Kind | Holds |
|---|---|---|
| `GameEngines5` | program | `Main.cpp`, and everything under `src/` and `include/` |
| `MemoryMonitorDemo` | program | `demo/MemoryMonitorDemo.cpp`, a walk through every path the monitor watches |
| `MemoryMonitorTests` | program | `tests/MemoryMonitorTests.cpp`, the monitor's checks |
| `EcsTests` | program | `tests/EcsTests.cpp`, the ECS checks |
| `ArenaTests` | program | `tests/ArenaTests.cpp`, the arena's checks |
| `ArenaDemo` | program | `demo/ArenaDemo.cpp`, the same work on the heap and from an arena |
| `EcsBench` | program | `bench/EcsBench.cpp`, a load for the profiler |
| `MathLibrary` | static library | `external/MathLibrary/include/` |

`EcsBench` and `ArenaDemo` are not tests. They measure rather than check, so CTest never launches
them. Run them by hand:

```powershell
.ind\Release\EcsBench.exe
.ind\Release\ArenaDemo.exe
```

Release, always. Debug is roughly fifteen times slower and says nothing about a shipped build.

Each program has its own `main()`, so each one is its own target. `cmake/CommonSettings.cmake`
holds what they share: C++20, the warning level, the include paths, and where output lands.

`MathLibrary` is a library because it is someone else's code, vendored under `external/`.

`MemoryMonitor` is not a target. It is two files, and each program that wants it compiles them,
which is the same thing this README tells anyone else to do with it. The engine picks them up
with the rest of `src/`; the demo and the tests name `src/utils/MemoryMonitor.cpp` on their own,
because they want the monitor and not the renderer.

Everything under `src/` and `include/` belongs to the engine, so adding a file there needs no
change to `CMakeLists.txt`. The build re-reads those folders each time.

Output goes to `bin/<platform>/<configuration>/`.

### Building

CMake, one build tree per platform, each holding both configurations:

```powershell
cmake --preset x64
cmake --build --preset x64-Debug
```

The presets are `x64` and `Win32`, and the build and test presets add the configuration:
`x64-Debug`, `x64-Release`, `Win32-Debug`, `Win32-Release`.

`CMakePresets.json` pins the generator, the toolset and the build directory. The toolset is
pinned on purpose. A machine with more than one set of Build Tools installed will otherwise take
whichever is newest, and the saved output snapshots were built with `v143`.

### Opening it in an IDE

Open `CMakeLists.txt`, not a solution file. Rider and Visual Studio both read
`CMakePresets.json`, so the profile list offers `x64` and `Win32` already filled in. Pick one of
those rather than letting the IDE invent a profile of its own: left to choose, it picks its own
build directory and whichever compiler is newest, and then a demo run out of the IDE can differ
from `scripts/capture-output.sh` for no visible reason.

`build/cmake-x64/GameEngines5.sln` is generated, so Visual Studio has a solution to open if you
want one. Never change project settings inside it. It is rewritten on every reconfigure, and it
is not in version control. `CMakeLists.txt` is the file to edit.

### Taking MemoryMonitor into another project

Two files, and no build system needed:

```
include/utils/MemoryMonitor.h
src/utils/MemoryMonitor.cpp
```

Put `include/` on the include path, compile the `.cpp`, and the monitor is watching before `main`
starts. It needs nothing else. Not the math library, not glm.

### The vendored libraries

`external/` holds two libraries copied in whole, so the repository builds on any machine with
Visual Studio and nothing else set up first:

- `external/MathLibrary/` - the course math library, 48 files
- `external/glm/` - glm 0.9.9, which the math library's `DebugUtlities` needs

Neither folder came with a license file. Add glm's before this repository goes anywhere public.

## MemoryMonitor

C++ programs ask for memory with `new` and hand it back with `delete`. Two mistakes are easy to make
and hard to find: asking for memory and never giving it back (a leak), and giving the same memory back
twice (a double free).

`MemoryMonitor` replaces `new` and `delete` for the whole program, so every allocation goes through it
first. It counts the bytes, prints a line for each call, and checks that each free is honest. In a Debug
build it also keeps a list of everything still alive, so it can tell you what leaked and which line of
your code made it.

There is nothing to set up. Include the header, compile `src/utils/MemoryMonitor.cpp`, and it is
already watching, even before `main` starts.

```cpp
#include "utils/MemoryMonitor.h"

int main()
{
    /* The runtime allocates before main, so measure from here */
    const MemoryStats start = MemoryMonitor::GetStats();

    int* kept = MEMMON_NEW int(42);   /* never deleted, on purpose */
    delete new int(1);                /* made and given back */

    MemoryMonitor::Report(start);     /* the numbers since start */
    MemoryMonitor::ReportLeaks(start);/* what is still alive, and where it came from */
}
```

```text
Operation: New    | new(size_t) file-tagged              | size = 4 bytes | live = 4 bytes
Operation: New    | new(size_t)                          | size = 4 bytes | live = 8 bytes
Operation: Delete | delete(void*, size_t)                | size = 4 bytes | live = 4 bytes
-- Memory Report --
Allocations         : 2
Frees               : 1
Bytes allocated     : 8 bytes
Bytes freed         : 4 bytes
Live bytes          : 4 bytes
Live blocks         : 1
...
-- Leak Report --
Leak:      | address = 0x1d4f2a0 | size = 4 bytes | Main.cpp:8
Leaked blocks       : 1
Leaked bytes        : 4 bytes
```

### What it does

#### 1. Catches every `new` and `delete`

It replaces all eight standard forms: scalar and array, plain and aligned, sized and unsized. Your own code, `std::vector`, `std::string` and the runtime all go through it.

```cpp
int* one = new int(5);       // new(size_t)
int* many = new int[8];      // new[](size_t)
delete one;                  // delete(void*, size_t)
delete[] many;               // delete[](void*)

std::vector<float> v(100);   // also counted, the vector calls operator new too
```

#### 2. Counts allocations from before `main`

The counters and the lock use `constinit`. They are ready before any code runs. So the runtime's own startup allocations are counted too.

```cpp
int main()
{
    // Not zero here: the runtime already allocated
    const MemoryStats atStart = MemoryMonitor::GetStats();
    std::printf("%llu allocations before main\n", atStart.allocationCount);
}
```

#### 3. Gives you a snapshot of the counters

`GetStats()` returns a copy. It holds running totals, the peak, the largest block, and two error counters. The copy is taken while the monitor is locked, so every field comes from the same moment, even while other threads allocate.

```cpp
const MemoryStats s = MemoryMonitor::GetStats();

s.allocationCount;    // total calls to new
s.freeCount;          // total calls to delete that really freed
s.bytesAllocated;     // total bytes asked for
s.bytesFreed;         // total bytes given back
s.peakLiveBytes;      // most memory alive at once
s.largestAllocation;  // biggest single block
s.badBlockCount;      // frees refused (Debug only)
s.mismatchCount;      // frees through the wrong delete
s.LiveBytes();        // bytes still allocated right now
s.LiveBlocks();       // blocks still allocated right now
```

The totals are 64-bit, so they do not wrap in a Win32 build.

#### 4. Prints a report since a point you choose

Save the stats at some point, then pass them to `Report()`. It prints only what happened after that point. The two error lines only show up when something went wrong.

```cpp
const MemoryStats baseline = MemoryMonitor::GetStats();

LoadLevel();
UnloadLevel();

MemoryMonitor::Report(baseline); // anything in "Live bytes" is a leak
```

```text
-- Memory Report --
Allocations         : 12
Frees               : 12
Bytes allocated     : 4096 bytes
Bytes freed         : 4096 bytes
Live bytes          : 0 bytes
Live blocks         : 0
...
```

Live bytes can go negative. That happens when code after the baseline frees memory that was allocated before it.

#### 5. Logs every call, in color

Each `new` and `delete` prints one line. It shows the operator, the size, the alignment and the live bytes. Each operator has its own color. Array forms use the brighter shade.

```cpp
int* p = new int(1);
delete p;
```

```text
Operation: New    | new(size_t)                          | size = 4 bytes | live = 4 bytes
Operation: Delete | delete(void*, size_t)                | size = 4 bytes | live = 0 bytes
```

#### 6. Lets you turn the log off

Logging starts on. You can switch it off for noisy parts. The counters keep running. `Report()`, `ReportLeaks()`, and the `Refused:`, `Mismatch:` and `Damaged:` lines always print.

```cpp
MemoryMonitor::SetLogging(false);
BuildNavMesh();                       // thousands of allocations, no log lines
MemoryMonitor::SetLogging(true);

if (MemoryMonitor::IsLogging()) { /* ... */ }
```

The flag is an atomic, so one thread can switch it while others allocate.

#### 7. Handles over-aligned types

Types with `alignas` above the default (16 on x64, 8 on Win32) go through the aligned overloads. The monitor uses `_aligned_malloc` for those. The pointer you get back is aligned as asked.

```cpp
struct alignas(64) CacheLine { std::uint64_t counter; };

CacheLine* c = new CacheLine;   // new(size_t, align_val_t)
// reinterpret_cast<std::uintptr_t>(c) % 64 == 0
delete c;
```

#### 8. Frees through the right path, even when the caller is wrong

Each block stores its own alignment in a header. `delete` reads the header to pick `free` or `_aligned_free`. It does not trust the caller. If the caller used the wrong delete form, the monitor prints a `Mismatch:` line, counts it, and still frees the block the right way. This works in Debug and Release.

```cpp
void* p = operator new(96, std::align_val_t{64});
operator delete(p);   // wrong: plain delete on an aligned block
```

```text
Mismatch:  free   | delete(void*)                        | address = 0x... | expected align 0, block says 64
```

#### 9. Catches double frees and bad pointers (Debug only)

In Debug, each block header ends with a check value. `delete` reads it first. A wrong value means the pointer was freed already, or never came from `new`. The monitor prints a `Refused:` line and does not free the block. Leaking one block is safer than corrupting the heap.

```cpp
int* p = new int(42);
delete p;
delete p;   // second time
```

```text
Refused:   free   | delete(void*, size_t)                | address = 0x... | bad check value: freed twice, or not from here
```

The same check catches a `delete` on a stack address:

```cpp
alignas(16) unsigned char storage[256] = {};
operator delete(storage + 128);   // refused, "not from here"
```

The check, the unlink and the "freed" stamp happen in one locked step. So two threads that free the same block at the same moment get the same answer: one frees it, the other is refused.

#### 10. Catches small writes just before a block (Debug only)

The check value sits in the last bytes of the header, right before your memory. A write one element too far back lands on it. The next `delete` sees the damage and refuses the free.

```cpp
int* grid = new int[100];
grid[-1] = 0;     // underrun, overwrites the check value
delete[] grid;    // Refused:
```

#### 11. Tells you the cost of the header

Every block has a header in front of it. It is 16 bytes on x64 Release and 64 on x64 Debug (half that on Win32). The Debug header is bigger because it also holds the leak list links, the file and line, and an allocation number. You can ask for the size, and ask if this build checks blocks.

```cpp
std::size_t overhead = MemoryMonitor::HeaderOverhead();   // bytes per block
bool checked = MemoryMonitor::HasCheckedHeaders();        // true in Debug
```

#### 12. Never allocates while it logs

Logging runs inside `operator new`. If it allocated, it would call itself forever. So it writes into a stack buffer with `std::format_to_n` and prints with `fputs`. It never uses `std::format` or `std::cout`. It is also safe during static destruction, after `std::cout` is gone.

```cpp
// Inside the monitor, the pattern every print follows:
char buffer[256];
auto end = std::format_to_n(buffer, sizeof(buffer) - 2, "size = {} bytes", size).out;
*end++ = '\n';
*end = '\0';
std::fputs(buffer, stdout);
```

The same rule holds for the lock: nothing inside it may allocate, or the monitor would wait for itself forever.

#### 13. Lists the blocks that leaked (Debug only)

Every live block sits in a list. `new` adds it, `delete` takes it out. `ReportLeaks()` walks that list and prints each block still alive, newest first. It gives the address, the size and, for aligned blocks, the alignment.

Pass a baseline to see only what leaked after that point. With no baseline it lists every live block, including the runtime's own from before `main`.

```cpp
const MemoryStats baseline = MemoryMonitor::GetStats();

LoadLevel();
UnloadLevel();

const LeakSummary leaks = MemoryMonitor::ReportLeaks(baseline);
if (leaks.listed > 0) { /* something was not freed */ }
```

```text
-- Leak Report --
Leak:      | address = 0x... | size = 64 bytes | alignment = 64 | (unknown)
Leak:      | address = 0x... | size = 4 bytes | (unknown)
Leaked blocks       : 2
Leaked bytes        : 68 bytes
```

It returns a `LeakSummary` with the number of blocks, their bytes, and whether the walk stopped early. Like `Report()`, it always prints, even with logging off, and it never allocates. The whole walk holds the lock, so it is safe to ask for a report while other threads allocate.

#### 14. Names the file and line of a leak, with `MEMMON_NEW` (Debug only)

Write `MEMMON_NEW` instead of `new`, and the block remembers where it was made. The leak report then shows the file and line instead of `(unknown)`.

```cpp
Enemy* e = MEMMON_NEW Enemy();       // tagged
Enemy* f = new Enemy();              // still works, shows (unknown)
```

```text
Leak:      | address = 0x... | size = 4 bytes | Main.cpp:227
```

It works for arrays and over-aligned types too. If a constructor throws, the memory is still given back. In Release, `MEMMON_NEW` is a plain `new`, and nothing is recorded.

Smart pointers go through the same operators, so they are counted too. `std::make_unique` writes its own `new` inside the standard library, which no file tag can reach, so its blocks say `(unknown)`. `MEMMON_MAKE_UNIQUE` makes the same object through the tagged `new` instead:

```cpp
auto plain  = std::make_unique<Enemy>(hp, name);   // leak report: (unknown)
auto tagged = MEMMON_MAKE_UNIQUE(Enemy, hp, name); // leak report: Level.cpp:118
```

It takes one object, not an array: for arrays use `std::make_unique<Enemy[]>(count)`, which stays untagged. In Debug it is one expression, so the pointer owns the object right away, exactly as safe as `make_unique`. In Release it *is* `std::make_unique`.

There is no `MEMMON_MAKE_SHARED`. `std::make_shared` packs the object and the reference counts into one block, and a tagged version would need two blocks instead. Writing `std::shared_ptr<Enemy>(MEMMON_NEW Enemy)` gets you the file and line at that price, if you want it.

#### 15. Tells you where a block came from (Debug only)

`GetAllocationSite()` reads the file and line stored for a pointer. It returns an empty site for a plain `new`, a pointer with a broken check value, or in Release.

```cpp
int* p = MEMMON_NEW int(1);
const AllocationSite site = MemoryMonitor::GetAllocationSite(p);
// site.file ends with "Main.cpp", site.line is the line of the new
```

Like `delete`, it is only safe on pointers that came from `new`.

#### 16. Stops safely at a damaged block (Debug only)

The leak report checks each block's check value before it moves on to the next one. If a bug wrote over a block's header, the report stops there instead of following a broken pointer. It prints a `Damaged:` line and sets `damaged` in the summary.

```cpp
int* grid = new int[100];
grid[-1] = 0;                        // underrun, breaks the check value
MemoryMonitor::ReportLeaks();
```

```text
Damaged:   | address = 0x... | bad check value, walk stopped: older blocks not listed
```

#### 17. Works when several threads allocate at once

One lock guards everything inside the monitor: the counters, the leak list, and the log line. A thread takes it, does its bookkeeping, and releases it, so the others wait their turn. The lock is a Windows `SRWLOCK`, which starts as all zeros. That means it works before `main`, and it never allocates memory of its own.

```cpp
std::thread a([] { for (int i = 0; i < 1000; ++i) delete new int; });
std::thread b([] { for (int i = 0; i < 1000; ++i) delete new int; });
a.join();
b.join();
// allocationCount grew by exactly 2000, and the leak list is intact
```

What that gives you:

- no count is lost, however many threads allocate
- the leak list never breaks, and you can ask for a leak report while threads are running
- a `GetStats()` copy is one moment, not a mix of moments
- each log line stays whole, instead of two threads splitting a line
- two threads that free the same block get one free and one `Refused:` line

The heap itself stays parallel: `malloc` and `free` run outside the lock, so only the bookkeeping takes turns.

### What it does not do

#### 1. It does not replace the `nothrow` overloads

`new (std::nothrow)` is not replaced. MSVC forwards it to the replaced `new`, so it is still counted there. Other compilers may not do that.

```cpp
int* p = new (std::nothrow) int(5);   // counted on MSVC only, by luck of the runtime
```

#### 2. It does not see memory that skips `operator new`

Direct calls to `malloc`, `_aligned_malloc`, `HeapAlloc` or `VirtualAlloc` go around it. So do custom allocators built on those.

```cpp
void* raw = std::malloc(1024);   // invisible to the monitor
std::free(raw);
```

#### 3. It does not name the file and line of every leak

Only blocks made with `MEMMON_NEW` record where they came from. A plain `new` shows `(unknown)`. That includes every allocation inside `std::vector`, `std::string` and other libraries, because their `new` is written inside the library.

```cpp
std::vector<int> v(100);             // leak report: (unknown)
auto q = std::make_unique<Thing>();  // leak report: (unknown)
int* p = MEMMON_NEW int(5);          // leak report: Main.cpp:42
auto r = MEMMON_MAKE_UNIQUE(Thing);  // leak report: Main.cpp:43
```

There is no call stack either, only the one file and line.

#### 4. It makes threads take turns

The safety in point 17 comes from one lock held by one thread at a time. So threads that allocate hard will wait for each other inside the monitor, and a heavily threaded program allocates slower than it would without it.

We measured the counter work with 4 threads on x64: about 46 ns per `new` and `delete` pair under the lock, against about 30 ns for a lock-free version that could not give a whole-picture snapshot. That was the trade we chose.

```cpp
// Four worker threads allocating in a tight loop will spend time waiting on each other
// inside the monitor, even though malloc itself still runs in parallel.
```

#### 5. It does not check anything in Release, or list leaks there

Release drops the check value and the leak list to keep the header small. A double free or a bad pointer goes straight to the C runtime, as it would without the monitor. `ReportLeaks()` only prints `needs a Debug build`. The counters, `Report()` and the wrong-operator check (point 8 above) still work, and they are thread safe in both builds.

```cpp
int* p = new int(42);
delete p;
delete p;                            // Debug: Refused. Release: undefined behavior, likely a crash.
MemoryMonitor::ReportLeaks();        // Release: "Leak list : needs a Debug build"
```

#### 6. It does not always name a double free as a double free

It has an "already freed once" message. But the C runtime often reuses the freed block right away and writes over the header. Then the monitor prints the vaguer "freed twice, or not from here". Both mean the same thing: do not touch that pointer.

```cpp
delete p;
delete p;   // usually "bad check value: freed twice, or not from here"
```

#### 7. It does not survive a truly wild pointer

To check a block, it must read the header in front of the pointer. If that address is not mapped, the read itself crashes before the monitor can report anything.

```cpp
operator delete(reinterpret_cast<void*>(0x10));   // crashes on the header read
```

#### 8. It does not catch overruns past the end

There is no guard after the block. Writing past the end goes unnoticed.

```cpp
int* grid = new int[100];
grid[100] = 0;    // one past the end, not detected
delete[] grid;    // frees normally
```

It also only guards the last 4 bytes of the header. An underrun that skips them is missed.

```cpp
int* grid = new int[100];
grid[-2] = 0;     // hits the padding, not the check value: not detected
delete[] grid;
```

#### 9. It does not catch `delete` vs `delete[]` mixups, or a wrong size

The mismatch check only compares alignment. Scalar and array forms, or a sized delete with the wrong size, look the same to it.

```cpp
int* many = new int[8];
delete many;                         // should be delete[], not reported

void* p = operator new(64);
operator delete(p, 9999);            // wrong size, ignored: the header size is used
```

#### 10. It does not catch use after free

Freed memory is not filled with a pattern. Reading or writing a freed block is not detected.

```cpp
int* p = new int(42);
delete p;
*p = 7;   // not detected
```

#### 11. It does not count the header in the byte totals

`bytesAllocated` is what callers asked for. The real heap use is higher by `HeaderOverhead()` per block, plus alignment padding.

```cpp
new char[1];   // bytesAllocated goes up by 1, the heap uses 1 + header bytes
```

#### 12. It does not scope the peak and largest block

`Report(baseline)` subtracts the baseline from the totals only. The peak and the largest block always cover the whole program.

```cpp
const MemoryStats baseline = MemoryMonitor::GetStats();
new char[16];
MemoryMonitor::Report(baseline);   // "Largest allocation" may still show a block from startup
```

#### 13. It does not print long lines in full, or print to a file

Log lines are cut at 254 characters, so the newline and the end marker always fit. Everything goes to `stdout`.

```cpp
MemoryMonitor::Report(baseline);   // stdout only, redirect the program to save it
```

#### 14. It does not build outside Windows and MSVC

It uses `_aligned_malloc`, `_aligned_free` from `<malloc.h>`, and an `SRWLOCK` from `<windows.h>`. Those are Microsoft only, and both builds need them.

```cpp
#include <malloc.h>    // _aligned_malloc / _aligned_free
#include <windows.h>   // SRWLOCK
```

#### 15. It does not list blocks behind a damaged one

When the leak report reaches a block with a broken check value, it stops. Every older block behind it stays hidden from that report and from every later one, because the broken block is never freed. The counters in `Report()` still count them.

```cpp
int* grid = new int[100];
grid[-1] = 0;                        // grid is now damaged, and delete refuses it
delete[] grid;
MemoryMonitor::ReportLeaks();        // stops at grid: older leaks are not listed
```

#### 16. It does not keep your own printing out of its lines

The lock keeps the monitor's own lines whole. It cannot do anything about your printing. If one thread writes to `std::cout` while another one allocates, the two can still land on the same line.

```cpp
std::thread a([] { std::cout << "loading level\n"; });
std::thread b([] { delete new int; });   // its log line can cut into that text
```

#### 17. It is not free

Every Debug block carries a 64 byte header on x64 (32 on Win32), and every `new` and `delete` updates the leak list. Both builds also take the lock on every call, as point 4 explains. That is fine for finding bugs, but the numbers you measure with the monitor are not the ones you get without it.

## Profiler

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

That answers the question `docs/ECS.md` section 8 raised about archetype churn. Adding a tag here
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

## Arena

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

## Running the tests

From the project root in PowerShell:

```powershell
.\scripts\run-tests.bat
```

This builds the `MemoryMonitorTests`, `EcsTests` and `ArenaTests` targets with CMake and runs them
through CTest in Debug and Release, for x64 and Win32. It configures a build tree first if there is not one already. CTest
prefixes each line of the program's own output with `1:`.

To run the tests without the script, against a tree that is already configured:

```powershell
ctest --preset x64-Debug
```

To run one configuration only:

```powershell
.\scripts\run-tests.bat x64 Debug
```

Red `Refused:`, `Mismatch:` and `Damaged:` lines are expected. Those tests make mistakes on purpose. Only `FAIL` means something broke.

`MemoryMonitorTests` is the one target built without `/sdl`, and it has to be. `/sdl` overwrites a
pointer with a junk address the moment it is deleted. The double free test deletes the same pointer
twice on purpose, so under `/sdl` the second delete hands the monitor a wild address instead of a
stale one, and reading the header there is an access violation. `CMakeLists.txt` says the same thing
next to the setting.

Each test starts with a `TEST CASE:` header and one plain line saying what it checks. Debug runs 81 checks. Release runs 42, because the checks that need a check value or the leak list are skipped there.

`EcsTests` and `ArenaTests` are their own programs, registered with CTest next to
`MemoryMonitorTests`, so `ctest --preset x64-Debug` runs all three. `EcsTests` runs 160 checks in
both configurations. `ArenaTests` runs 73 in Debug and 72 in Release, because the check that reads
the `0xDD` fill only exists in Debug.

Four of the tests start threads: they hammer the counters and the leak list from 4 threads at once, ask for reports while that happens, and race two frees on the same block. They repeat their work 3 times per run. Timing bugs do not show up every run, so it is worth running the tests a few times in a row.

## Checking that a refactor changed nothing

`scripts/capture-output.sh` builds all four configurations of `MemoryMonitorDemo`, runs them, and
saves the output. It replaces heap addresses with `0xADDR`, so runs can be compared.

One thing to know: in Debug the demo prints its own file name and line numbers, from `MEMMON_NEW`
and `MEMMON_MAKE_UNIQUE`. Add or remove a line in `demo/MemoryMonitorDemo.cpp` and those lines shift,
even when nothing else changed. Release prints no file tags, so it stays a clean comparison.

```bash
./scripts/capture-output.sh step0          # before the change
./scripts/capture-output.sh step1 step0    # after, and compare
```
