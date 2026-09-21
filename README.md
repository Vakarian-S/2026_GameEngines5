# GameEngines5

## MemoryMonitor

`MemoryMonitor` replaces the global `operator new` and `operator delete`. Every heap allocation in the program goes through it. It counts the bytes, logs each call, and checks each free.

Include it with:

```cpp
#include "utils/MemoryMonitor.h"
```

There is nothing to set up. Linking `src/utils/MemoryMonitor.cpp` into the program is enough.

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

The counters use `constinit`. They are zero before any code runs. So the runtime's own startup allocations are counted too.

```cpp
int main()
{
    // Not zero here: the runtime already allocated
    const MemoryStats atStart = MemoryMonitor::GetStats();
    std::printf("%llu allocations before main\n", atStart.allocationCount);
}
```

#### 3. Gives you a snapshot of the counters

`GetStats()` returns a copy. It holds running totals, the peak, the largest block, and two error counters.

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

Logging starts on. You can switch it off for noisy parts. The counters keep running. `Report()`, `Refused:` and `Mismatch:` lines always print.

```cpp
MemoryMonitor::SetLogging(false);
BuildNavMesh();                       // thousands of allocations, no log lines
MemoryMonitor::SetLogging(true);

if (MemoryMonitor::IsLogging()) { /* ... */ }
```

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

#### 10. Catches small writes just before a block (Debug only)

The check value sits in the last bytes of the header, right before your memory. A write one element too far back lands on it. The next `delete` sees the damage and refuses the free.

```cpp
int* grid = new int[100];
grid[-1] = 0;     // underrun, overwrites the check value
delete[] grid;    // Refused:
```

#### 11. Tells you the cost of the header

Every block has a header in front of it. It is 16 bytes on x64 Release, 32 on x64 Debug (half that on Win32). You can ask for the size, and ask if this build checks blocks.

```cpp
std::size_t overhead = MemoryMonitor::HeaderOverhead();   // bytes per block
bool checked = MemoryMonitor::HasCheckedHeaders();        // true in Debug
```

#### 12. Never allocates while it logs

Logging runs inside `operator new`. If it allocated, it would call itself forever. So it writes into a stack buffer with `std::format_to_n` and prints with `fputs`. It never uses `std::format` or `std::cout`. It is also safe during static destruction, after `std::cout` is gone.

```cpp
// Inside the monitor, the pattern every print follows:
char buffer[256];
auto end = std::format_to_n(buffer, sizeof(buffer) - 1, "size = {} bytes", size).out;
*end = '\0';
std::fputs(buffer, stdout);
```

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

#### 3. It does not tell you where a leak came from

It counts leaked bytes and blocks. It does not store a file, a line or a call stack. It cannot list the leaked blocks.

```cpp
const MemoryStats baseline = MemoryMonitor::GetStats();
int* leaked = new int(42);
MemoryMonitor::Report(baseline);   // "Live blocks : 1", but not which one
```

#### 4. It is not thread safe

The counters are plain integers, not atomics. There is no lock. Two threads that allocate at the same time can lose counts.

```cpp
std::thread a([] { for (int i = 0; i < 1000; ++i) delete new int; });
std::thread b([] { for (int i = 0; i < 1000; ++i) delete new int; });
// allocationCount may grow by less than 2000
```

#### 5. It does not check anything in Release

Release drops the check value to keep the header small. A double free or a bad pointer goes straight to the C runtime, as it would without the monitor. The wrong-operator check (point 8 above) still works.

```cpp
int* p = new int(42);
delete p;
delete p;   // Debug: Refused. Release: undefined behavior, likely a crash.
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

Log lines are cut at 256 characters. Everything goes to `stdout`.

```cpp
MemoryMonitor::Report(baseline);   // stdout only, redirect the program to save it
```

#### 14. It does not build outside Windows and MSVC

It uses `_aligned_malloc`, `_aligned_free` and `<malloc.h>`. Those are Microsoft only.

```cpp
#include <malloc.h>   // _aligned_malloc / _aligned_free
```

## Running the tests

From the project root in PowerShell:

```powershell
.\scripts\run-tests.bat
```

This builds and runs the MemoryMonitor tests in Debug and Release, for x64 and Win32. To run just one:

```powershell
.\scripts\run-tests.bat x64 Debug
```

Red `Refused:` and `Mismatch:` lines are expected. Those tests make mistakes on purpose. Only `FAIL` means something broke.

## Checking that a refactor changed nothing

`scripts/capture-output.sh` builds all four configurations of the demo, runs them, and saves the output. It replaces heap addresses with `0xADDR`, so runs can be compared.

```bash
./scripts/capture-output.sh step0          # before the change
./scripts/capture-output.sh step1 step0    # after, and compare
```
