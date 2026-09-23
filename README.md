# GameEngines5

A small C++ game engine project, built step by step. Right now it holds one finished piece:
`MemoryMonitor`, a tool that watches every piece of memory the program takes and gives back.

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
- [Running the tests](#running-the-tests)
- [Checking that a refactor changed nothing](#checking-that-a-refactor-changed-nothing)

## Layout

Three programs and one library.

| Target | Kind | Holds |
|---|---|---|
| `GameEngines5` | program | `Main.cpp`, and everything under `src/` and `include/` |
| `MemoryMonitorDemo` | program | `demo/`, a walk through every path the monitor watches |
| `MemoryMonitorTests` | program | `tests/`, the checks |
| `MathLibrary` | static library | `external/MathLibrary/include/` |

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

## Running the tests

From the project root in PowerShell:

```powershell
.\scripts\run-tests.bat
```

This builds the `MemoryMonitorTests` target with CMake and runs it through CTest in Debug and
Release, for x64 and Win32. It configures a build tree first if there is not one already. CTest
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
