# GPU timestamps

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed 2026-11-11 and built 2026-11-12, as step 1 of the ray query shadows plan (commit `1a9c9fa`).
Step 2, decision e made timestamps optional and hid the GPU times on a GPU without them (commit
`cf3496c`), and step 5 added the `TlasBuild` zone (commit `feab448`). "Step N" below means a step of the ray query shadows plan, which ended on 2026-11-14; README.md maps each one
to its note. Each section below is the step's whole plan section.

The plan's goal: hard shadows from ray queries, measured with these timers. ray-query-shadows.md holds
the shadow steps. tools/gpu-queries.md is the current manual for the timers.

## Step 1: GPU timestamps around the mesh pass

**The question it answers:** how long did the GPU spend running the mesh draws? The mesh pass is
`RecordMeshPass` in `FrameLoop.cpp`, once per frame, inside the one rendering block with the line pass.
The window covers every pipeline stage those draws run: vertex, tessellation and geometry if present,
rasterization, fragment, depth and blending. It leaves out CPU time, the clear, the line pass and the
barriers. It gives one total, not a split per stage; Nsight or RenderDoc split it. The CPU time to
record the commands is `PROFILE_SCOPE("Record")`, a different clock.

**Decisions**

- **a. Permanent, behind `GE5_PROFILING`.** The same switch as the CPU profiler, so a shipping build
  removes both. Release is not the shipping build: measuring happens in Release.
- **b. Named zones.** Each zone is a pair of timestamps with a name. Only `"Mesh pass"` now; step 5 adds
  `"TLAS build"`. Other zones (line pass, compute) wait for a question they answer. An enum,
  `GPUTimerName`, names the zones; its `Count` sizes the query pool at two queries per value.
- **c. A scope object and a macro.** `PROFILE_GPU_SCOPE(queries, commandBuffer, name)` creates a
  `ScopedGPUTimer`, named to match the CPU profiler's `ScopedTimer` and `ScopedEcsTimer`. Its constructor records the start timestamp write and its destructor the end write, so
  the end is never forgotten. With the switch at 0 the macro expands to nothing. It reuses
  `GE5_PROFILE_JOIN` from `Profiler.h`, and lives in a Vulkan header because it names
  `VkCommandBuffer`. Tracy's `TracyVkZone` works the same way.
- **d. One query pool per frame slot.** Two timestamp queries per zone. Per slot, because the CPU reads
  one slot's results while the GPU may write the other's.
- **e. Stages.** `TOP_OF_PIPE` for the start, `BOTTOM_OF_PIPE` for the end, written with
  `vkCmdWriteTimestamp2`. The Khronos sample says this pair gives approximate but correct timing on most
  GPUs, and intermediate stages give meaningless results.
- **f. Reset in the command buffer.** `RecordReset` records `vkCmdResetQueryPool` first in
  `RecordCommandBuffer`, before the image barrier. A reset is not allowed inside a rendering block.
  This matches the Khronos sample and needs no feature.
- **g. Read after the fence.** `ReadResults` runs in `FrameLoop::DrawFrame` right after the slot's fence
  wait, so the GPU has finished that slot and the read never stalls. Results are 64-bit
  (`VK_QUERY_RESULT_64_BIT`), masked with the queue's `timestampValidBits`, and converted with
  `(end - start) * timestampPeriod / 1,000,000` to ms.
- **h. Output.** `FrameLoop` measures. `VulkanRenderer::GetMeshPassMilliseconds` returns the average over
  the second as a plain `double`, with no Vulkan type. `DrawFrame` stays `void`: the time it has is
  from one or two frames ago, not this frame. `Application` adds the value to its once-a-second line.
- **i. Average only.** Shadow cost is steady, so the average answers whether performance got worse. Min
  and max wait until spikes become a question. The Khronos sample shows one value per frame.
- **j. Support check.** `GPUContext` checks that the graphics queue family's `timestampValidBits` and the
  device's `timestampPeriod` are not zero.

**Names**

| what it is | name |
|---|---|
| The class that owns the query pools, resets, reads and averages | `GPUQueries` |
| The scope object that records the start and end writes | `ScopedGPUTimer` |
| The enum that names each zone | `GPUTimerName`, first value `MeshPass`, last `Count` |
| The macro | `PROFILE_GPU_SCOPE` |
| The header | `include/vulkan/GPUQueries.h` |
| The method that records the reset | `RecordReset` |
| The method that records one timestamp write into the current slot's pool | `WriteTimestamp` |
| The per-slot flag: a reset was recorded and that frame is not read yet | `resetRecorded` |
| The method that reads a slot after its fence wait | `ReadResults` |
| The method that returns one zone's average and clears it | `TakeAverageMilliseconds` |
| The `FrameLoop` and `VulkanRenderer` getters | `GetMeshPassMilliseconds` |
| The mask and ticks-to-ms math, with no Vulkan type so the tests build on Win32 | `include/core/TimestampMath.h`: `TimestampMask`, `TimestampMilliseconds` |
| The zone's name | `GPUTimerName::MeshPass`; the stats line prints `mesh pass` |

`GPUQueries` names what the class holds, and it stays true if pipeline statistics queries join later:
they also wrap a range of commands, so `ScopedGPUTimer` could record both, in a second pool per slot.

**Rejected**

- **Returning the time from `DrawFrame`.** The value is not this frame's.
- **Reset from the CPU with `vkResetQueryPool`.** Valid core Vulkan 1.2 behind the `hostQueryReset`
  feature, and this machine's RTX 4060 and its second GPU both report it. Rejected because the command
  buffer reset needs no feature and matches the official sample.
- **Printing from `FrameLoop`.** Backend code hands out numbers; only `app` prints.
- **`ReadAndResetResults`** as the read's name. The read resets nothing on the GPU, and "reset" stays
  the word for the pool reset only.

**What the build changed**

- **`GPUQueries`** is a member of `FrameLoop`. Without timestamp support it creates no pools, and every
  method returns at once. It keeps one sum and one count per `GPUTimerName`, so step 5's zone needs no
  new fields.
- **`resetRecorded`** stops two bad reads: a slot's first frame, whose pool was never reset (invalid
  to read), and a second read of the same frame after an early return from `DrawFrame`.
- **`TakeAverageMilliseconds`** returns 0 when nothing was measured, and clears the sum, so each
  once-a-second read covers one second.
- **`RecordCommandBuffer` lost its `const`,** because the reset and the scope change `GPUQueries`.
- **The mesh pass call sits in its own braces,** so the end write lands before the line pass.
- **The `RecordReset` and `ReadResults` calls sit inside `#if GE5_PROFILING`.** The macro already
  vanishes with the switch at 0; these two calls do not.
- **`GPUContext`** reads `timestampPeriod` and the graphics family's `timestampValidBits` at the end of
  `PickPhysicalDevice`, with `SupportsTimestamps` and two getters.
- **Not done:** on a GPU without timestamps the line prints `mesh pass 0.00 ms`. Step 2, decision e
  hides it.

**What the build checked**

- **Tests.** `scripts\run-tests.bat` passes in all four configurations. `EcsTests` gained 11 checks in a
  "GPU timestamp math" section, 337 in all: the mask for 64, 36, 1 and 0 valid bits; ticks to ms at
  1 ns and 0.5 ns; equal start and end read 0; a 36-bit and a 64-bit counter that wrap between start
  and end still read 1 ms, and the 36-bit wrap without the mask reads a huge time; a bit above the
  valid ones is ignored.
- **Scene1, Release, FIFO, 40 s** (`run-game.ps1 -Scene 1 -Config Release`, M pressed twice): exit 0,
  0 warnings, 0 errors, 0 asserts, 0 frame-arena heap fallbacks. Mesh pass 0.01 to 0.04 ms.
- **Scene0, Release, 30 s FIFO then 30 s IMMEDIATE** (Ctrl+P three times): the same clean summary.
  Mesh pass 0.07 to 0.23 ms under FIFO, 0.03 to 0.05 ms under IMMEDIATE.
- **Scene0, Scene1, Scene0, Debug** (F2 and F1): exit 0, 0 warnings, 0 errors, 0 asserts, 0 leaked
  blocks.
- **Not checked:** M did not hide the meshes in Scene0 during the run, so "meshes off reads near 0" was
  never seen.

**What the build found**

- **FIFO inflates GPU times.** Under FIFO the GPU draws Scene0 in about 0.1 ms, then idles up to 4 ms
  for the display, and the driver lowers its clock. The mesh pass rose and fell between 0.07 and
  0.23 ms in waves of 8 to 10 s, peaking in the seconds when the frame times turned uneven. The same
  scene under IMMEDIATE held 0.03 to 0.05 ms. Step 7, decision e already planned to measure without
  FIFO; this confirms it.
- **This machine's timestamp limits** (`vulkaninfo`): the RTX 4060 reports a 1 ns period and 64 valid
  bits on its graphics, compute and transfer queues, but 32 on video decode and encode, which wrap
  every 4.3 s. The Radeon 610M reports 10 ns and 64 bits on graphics, and 0 on two queues. On the
  engine's queue the mask keeps every bit; it guards other GPUs and queues.

**Sources:** Khronos sample "Timestamp queries"
(https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html), the
`vkCmdWriteTimestamp` and `vkResetQueryPool` reference pages, Tracy's `TracyVulkan.hpp`.

Seam: every later choice gets measured with this, such as shadow maps against ray query.
