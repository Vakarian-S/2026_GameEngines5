# GPU queries

## The problem

The CPU profiler times the code that records commands. It cannot time the GPU running them: the GPU
runs a frame's commands after the submit, often while the CPU records the next frame. The ray query
shadows plan needs GPU time to compare shadows on and off, and later shadow maps against ray query.

## Before and after

Before, the once-a-second stats line showed only frame times. Now it also shows the GPU time of the
mesh pass, averaged over the same second:

```text
IMMEDIATE | cap off | 3204.9 fps | 0.31 ms (min 0.10, max 1.59) | mesh pass 0.04 ms
```

Measured on 2026-11-12 in Scene0 (1,000 cubes), x64 Release, RTX 4060, with
`scripts\run-game.ps1 -Scene 0 -Config Release`: 0.03 to 0.05 ms under IMMEDIATE, and 0.07 to 0.23 ms
under FIFO. FIFO reads higher because the GPU idles between frames and lowers its clock.

## How it works

`GPUQueries` owns one timestamp query pool per frame slot. A pool is a fixed array of result slots
that the GPU writes into. Each timer takes two of them: a start and an end.

Each frame, `FrameLoop` does three things with it:

1. **Read.** Right after the slot's fence wait, `ReadResults(slot)` reads that slot's timestamps from
   two frames ago. The fence proves the GPU finished them, so the read never waits.
2. **Reset.** `RecordReset(commandBuffer, slot)` records the pool reset first in the command buffer,
   outside the rendering block.
3. **Time.** `PROFILE_GPU_SCOPE` wraps the commands to time. It creates a `ScopedGPUTimer`, whose
   constructor records a start write at `TOP_OF_PIPE` and whose destructor records an end write at
   `BOTTOM_OF_PIPE`.

```cpp
{
    PROFILE_GPU_SCOPE(gpuQueries, commandBuffer, GPUTimerName::MeshPass);
    RecordMeshPass(commandBuffer, pipelines.meshPipeline, resourceStore, drawList);
}
```

The braces matter: the end write is recorded where the timer is destroyed, so without them the line
pass would land inside the window.

`ReadResults` turns each pair into milliseconds with `TimestampMilliseconds`: end minus start, masked
to the queue's valid bits, times the tick length. `TakeAverageMilliseconds(name)` returns the average
since its last call and clears it. `Application` calls it through
`VulkanRenderer::GetMeshPassMilliseconds` once a second.

**Adding a timer:** add a value to `GPUTimerName` before `Count`, and wrap its commands in
`PROFILE_GPU_SCOPE`. The pool size, the read and the averages follow from `Count`. Add a getter
through `FrameLoop` and `VulkanRenderer` only when something prints it.

## Files

- `include/vulkan/GPUQueries.h`, `src/vulkan/GPUQueries.cpp`: `GPUTimerName`, `GPUQueries`,
  `ScopedGPUTimer` and the `PROFILE_GPU_SCOPE` macro.
- `include/core/TimestampMath.h`: `TimestampMask` and `TimestampMilliseconds`. It has no Vulkan type,
  so `EcsTests` can test it on Win32 too.
- `include/vulkan/GPUContext.h`, `src/vulkan/GPUContext.cpp`: `SupportsTimestamps`,
  `GetTimestampPeriod` and `GetTimestampValidBits`, read at the end of `PickPhysicalDevice`.
- `include/vulkan/FrameLoop.h`, `src/vulkan/FrameLoop.cpp`: owns the `GPUQueries` and makes the read,
  the reset and the mesh pass scope.
- `tests/EcsTests.cpp`: the "GPU timestamp math" section, 11 checks.

## Decisions while building

- **The same switch as the CPU profiler.** With `GE5_PROFILING` at 0 the macro expands to nothing, and
  `#if` blocks remove the read and the reset.
- **One pool per frame slot.** The CPU reads one slot's pool while the GPU may write the other's.
- **Reset in the command buffer, not from the CPU.** It needs no feature, and it matches the Khronos
  sample.
- **`resetRecorded` per slot.** Reading a pool that was never reset is invalid, which happens on each
  slot's first frame. `ReadResults` clears the flag, so an early return from `DrawFrame` never counts
  the same frame twice.
- **A frame with a missing write counts for nothing.** `vkGetQueryPoolResults` then returns
  `VK_NOT_READY`, and `ReadResults` skips the frame.
- **Average only.** The question is whether the GPU got slower, and the average answers it. Min and max
  wait until spikes become a question.
- **Optional.** A GPU whose graphics queue reports 0 valid bits, or a period of 0, gets no pools, and
  every method returns at once.

## Questions a reader might ask

### Why TOP_OF_PIPE for the start and BOTTOM_OF_PIPE for the end?

The GPU overlaps commands: later commands start before earlier ones finish. A timestamp write at a
stage waits until every earlier command has finished that stage. `TOP_OF_PIPE` waits for nothing, so
the start marks when the GPU reaches the timed commands. `BOTTOM_OF_PIPE` waits for every earlier
command to finish completely, so the end marks when the timed commands are done. The Khronos sample
calls this pair approximate but correct, and says stages in between give meaningless results.

### Why mask the timestamps?

A GPU's counter can have fewer than 64 bits, and it wraps to 0 after its biggest value. A wrap between
start and end makes `end - start` borrow into the bits the counter does not have. The mask removes
the borrow. On this machine the RTX 4060's graphics queue has all 64 bits, so the mask changes
nothing there; its video queues have 32, which wrap every 4.3 s at 1 ns per tick.

### Why are the times higher under FIFO?

Under FIFO the GPU finishes a frame and then waits for the display. A GPU that idles most of the time
lowers its clock, and the same commands take longer. Measure with IMMEDIATE or MAILBOX and the frame
cap off. Ctrl+P cycles the present mode.

### Does it time the line pass, the clear or the barriers?

No. Only the commands inside a scope are timed, and only the mesh pass has one.

### What does it cost?

Not measured. Per frame it records one reset and two writes, and makes one read on the CPU.

## More detail

The design and its sources: PLAN.md, step 1 (dev-notes once the plan is archived).
