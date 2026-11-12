# Plan

**Last updated:** 2026-11-12.

This plan adds hard shadows from ray queries. The fragment shader casts one ray from each surface point
toward each light and darkens that light when something blocks the ray. The course assignment asks for
shadows; ray query was chosen over shadow maps, and shadow maps stay a later fallback. No ROADMAP.md
block covers this; the plan came from the assignment and the design talk of 2026-11-11.

The plan builds only ray query shadows, but it keeps seams open for the later features listed at the
end: shadow maps, a GPU scene buffer, a compute fill of the TLAS, and ambient occlusion. Step 7 ends
with a bonus, non-uniform scale, which is independent of the shadows; it is here so step 7's floor can
become a flat box.

## Steps

| # | step | done when | stage |
|---|---|---|---|
| 1 | GPU timestamps around the mesh pass | The once-a-second stats line shows the mesh pass GPU time in ms | designed |
| 2 | Ray query support on the device | The device starts with the ray query extensions and `bufferDeviceAddress`, the new functions load, and validation stays clean | designed |
| 3 | Mesh buffers ready for acceleration structure builds | Vertex and index buffers carry the build flags, and `Buffer` returns its device address | designed |
| 4 | `AccelerationStructure` module and one BLAS per mesh | Each `Mesh` holds a BLAS built at load, and validation stays clean | designed |
| 5 | TLAS each frame, split into fill and build | The CPU fills instances from the `DrawList`, the build reads only the instance buffer, and the TLAS is ready before the mesh pass | designed |
| 6 | Shadows in the shader | The TLAS is bound in set 0, lighting calls only `shadowFactor`, a shadows on/off switch works, and in `Scene1` a shadow from Mario or a cube lands on Mario or a cube | designed |
| 7 | Fix artifacts and measure, with a floor in `Scene1`; bonus: non-uniform scale | No self-shadowing acne, shadows on/off GPU times are recorded, and as the bonus the floor is a flat box | designed |

**Stages**, in order. The cell says what the step is ready for, and it changes only when this file
is edited. Prototype, build and recap happen in the code and the chat; WORKFLOW.md describes them.

| stage | written when | meaning |
|---|---|---|
| outline | the plan is made | the step's idea and its reason, as it came from ROADMAP.md, not designed yet |
| designed | the design talk ends | the decisions are in the step's section, ready to build |
| done | the commit lands | built, recapped and committed. Write the commit hash in the stage cell |

A prototype's results go to `findings/`, and the step's section links to them.

When every step is done, each step's section becomes a dev-note, STATE.md is updated, and this file
starts again from the template.

---

## 1. GPU timestamps around the mesh pass

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
  `"TLAS build"`. Other zones (line pass, compute) wait for a question they answer.
- **c. A scope object and a macro.** `PROFILE_GPU_SCOPE(queries, commandBuffer, name)` creates a
  `GPUZone`. Its constructor records the start timestamp write and its destructor the end write, so
  the end is never forgotten. With the switch at 0 the macro expands to nothing. It reuses
  `GE5_PROFILE_JOIN` from `Profiler.h`, and lives in a Vulkan header because it names
  `VkCommandBuffer`. Tracy's `TracyVkZone` works the same way.
- **d. One query pool per frame slot.** Two timestamp queries per zone. Per slot, because the CPU reads
  one slot's results while the GPU may write the other's.
- **e. Stages.** `TOP_OF_PIPE` for the start, `BOTTOM_OF_PIPE` for the end, written with
  `vkCmdWriteTimestamp2`. The Khronos sample says this pair gives approximate but correct timing on most
  GPUs, and intermediate stages give meaningless results.
- **f. Reset in the command buffer.** `BeginFrame` records `vkCmdResetQueryPool` first in
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
| The scope object that records the start and end writes | `GPUZone` |
| The macro | `PROFILE_GPU_SCOPE` |
| The header | `include/vulkan/GPUQueries.h` |
| The method that records the reset | `BeginFrame` |
| The method that reads a slot after its fence wait | `ReadResults` |
| The `VulkanRenderer` getter | `GetMeshPassMilliseconds` |
| The zone's name string | `"Mesh pass"` |

`GPUQueries` names what the class holds, and it stays true if pipeline statistics queries join later:
they also wrap a range of commands, so `GPUZone` could record both, in a second pool per slot.

**Rejected**

- **Returning the time from `DrawFrame`.** The value is not this frame's.
- **Reset from the CPU with `vkResetQueryPool`.** Valid core Vulkan 1.2 behind the `hostQueryReset`
  feature, and this machine's RTX 4060 and its second GPU both report it. Rejected because the command
  buffer reset needs no feature and matches the official sample.
- **Printing from `FrameLoop`.** Backend code hands out numbers; only `app` prints.

**Sources:** Khronos sample "Timestamp queries"
(https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html), the
`vkCmdWriteTimestamp` and `vkResetQueryPool` reference pages, Tracy's `TracyVulkan.hpp`.

Seam: every later choice gets measured with this, such as shadow maps against ray query.

## 2. Ray query support on the device

**The problem:** `GPUContext` creates the device with one extension (`kDeviceExtensions` holds only the
swapchain) and a feature chain of the base features plus `Vulkan13Features`. `IsDeviceSuitable`
rejects any GPU that lacks a required extension or feature, and `PickPhysicalDevice` takes the first
suitable GPU. Ray query needs three more extensions (`VK_KHR_acceleration_structure`,
`VK_KHR_ray_query`, `VK_KHR_deferred_host_operations`), more feature structs, and extension functions
that `vulkan-1.lib` does not export.

**Decisions**

- **a. Ray query is optional.** `IsDeviceSuitable` keeps its current rules. `GPUContext` records whether
  the chosen GPU supports ray query, and enables the three extensions and their features only when
  it does. A GPU without ray query still draws the game, without shadows; the shadows switch from
  step 6 handles "off". Requiring it would refuse to start on GPUs older than ray tracing hardware,
  and no driver update fixes that.
- **b. The first suitable GPU stays.** Preferring a GPU with ray query, or a discrete one, is shipping
  work. On this machine the RTX 4060 is listed first; the AMD Radeon 610M also reports ray query.
- **c. Three feature structs join the existing `pNext` chain.** `VkPhysicalDeviceVulkan12Features` with
  `bufferDeviceAddress`, `VkPhysicalDeviceAccelerationStructureFeaturesKHR` with
  `accelerationStructure`, and `VkPhysicalDeviceRayQueryFeaturesKHR` with `rayQuery`. The same chain
  asks `vkGetPhysicalDeviceFeatures2` what the GPU supports, then goes to `vkCreateDevice` with the
  wanted members set. Order in the chain does not matter. With `Vulkan12Features` in the chain, the
  standalone 1.2 structs such as `VkPhysicalDeviceBufferDeviceAddressFeatures` must not be: the spec
  allows one of the two. Tutorial code often chains the standalone one, so copying it fails
  validation. The same rule already applies to `Vulkan13Features` and the standalone
  `VkPhysicalDeviceDynamicRenderingFeatures`.
- **d. A small struct of function pointers, owned by `GPUContext`.** After `vkCreateDevice`, and only
  when ray query is supported, `vkGetDeviceProcAddr` fills it with the ray tracing functions, about
  six. Otherwise the pointers stay null, and nothing calls them because the shadow code checks the
  support flag first.
- **e. Timestamps are optional too.** This is step 1, decision j. If the graphics queue family reports
  `timestampValidBits = 0` or the device reports `timestampPeriod = 0`, `GPUContext` records "no
  timestamps", `GPUQueries` records no writes, and the stats line shows no GPU time. A measuring tool
  never stops the game from starting.

**Rejected**

- **volk**, the MIT library that loads every Vulkan function. It changes where every Vulkan call comes
  from (`VK_NO_PROTOTYPES`, no `vulkan-1.lib`) to solve a problem the engine does not have: the engine
  is 64-bit only, so linking `vulkan-1.lib` works. The SDK on this machine does not ship it. Worth it
  if the extension list grows a lot.
- **Ray query as a requirement.** See decision a.

**Context:** the engine already requires Vulkan 1.3 (`apiVersion = VK_API_VERSION_1_3`). Vulkan is
implemented by the player's driver, so "update your graphics driver" is the fix for a GPU that can run
1.3, but no update adds ray tracing hardware.

Seam: shadows, ambient occlusion and reflections all reuse this setup.

## 3. Mesh buffers ready for acceleration structure builds

**The problem:** a BLAS build reads a mesh's vertex and index buffers by GPU address. `Mesh`'s
constructor in `ResourceStore.cpp` creates them with only `VERTEX_BUFFER_BIT` or `INDEX_BUFFER_BIT`
(the upload constructor adds `TRANSFER_DST_BIT`), `Buffer` allocates with a plain
`VkMemoryAllocateInfo`, and `Buffer` cannot return its address. The vertex layout already suits a
BLAS: `position` is three floats at offset 0, and the stride is `sizeof(Vertex)`.

**Decisions**

- **a. The two new flags only when ray query is supported.** `Mesh` adds
  `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` and
  `VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR` when `GPUContext` reports ray
  query support. The build input flag belongs to the acceleration structure extension, which step 2
  enables only on supported GPUs.
- **b. `Buffer` sets the memory allocate flag by itself.** When the usage contains
  `SHADER_DEVICE_ADDRESS_BIT`, the constructor chains a `VkMemoryAllocateFlagsInfo` with
  `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT` onto the allocate info. `Buffer` is the one class that sees
  both the usage and the allocation, so no caller can forget it, and later buffers (the TLAS instance
  buffer, a GPU scene buffer) get it for free. The Khronos "Buffer device address" sample chains the
  same struct, and VMA adds the flag automatically from the usage.
- **c. `Buffer::GetDeviceAddress()` asks each time and stores nothing.** It calls
  `vkGetBufferDeviceAddress`. A buffer's address never changes, the calls are few (two per mesh at
  load, about two per frame), and a stored field would hold a meaningless zero on buffers without the
  flag. Calling it on such a buffer is invalid, and validation reports it. The BLAS address is the
  opposite case: it is read once per instance per frame, so step 4 stores it.

**Sources:** Khronos sample "Buffer device address"
(https://docs.vulkan.org/samples/latest/samples/extensions/buffer_device_address/README.html), the
`vkGetBufferDeviceAddress` reference page, VMA's "Enabling buffer device address" page.

## 4. `AccelerationStructure` module and one BLAS per mesh

**The problem:** each mesh needs its triangles sorted into a BLAS, once, so rays can find them. A
BLAS is a buffer the driver lays out plus a handle, and a TLAS instance points at it by device
address. The mesh cache in `ResourceStore` means one BLAS per mesh path, however many entities use
it. The renderer owns the module; shadow code only reads it. A `Mesh` gets a BLAS only when
`GPUContext` reports ray query support (step 2, decision a).

**Decisions**

- **a. One `AccelerationStructure` class for BLAS and TLAS.** It holds the storage buffer, the handle,
  and the device address. The address comes from `vkGetAccelerationStructureDeviceAddressKHR` once at
  creation and is stored, because the TLAS fill reads it once per instance per frame. NVIDIA's
  tutorial uses the same shape: one struct with handle, buffer and address for the BLAS list and the
  TLAS. The Khronos tutorial keeps separate variables, a tutorial shortcut.
- **b. One build per mesh at load, the Khronos way.** Each BLAS gets its own one-time command buffer
  from `TemporaryCommandBuffers`, and `SubmitAndWait` returns once the build is done. NVIDIA batches
  builds, mainly to stay inside a memory budget; with a handful of cached meshes, that buys little.
  The triangles are described as `VK_FORMAT_R32G32B32_SFLOAT` positions at offset 0 with a stride of
  `sizeof(Vertex)`, and `VK_INDEX_TYPE_UINT32` indices, as in the Khronos tutorial.
- **c. `PREFER_FAST_TRACE` on the build, `OPAQUE` on the geometry, no compaction yet.**
  - `PREFER_FAST_TRACE` (in `VkAccelerationStructureBuildGeometryInfoKHR::flags`) asks the driver for
    a tight tree that rays cross quickly, at a slower build. A static mesh is built once and traced
    every frame. It cannot be combined with `PREFER_FAST_BUILD`.
  - `OPAQUE` (in `VkAccelerationStructureGeometryKHR::flags`) says every triangle is solid, so a ray
    query commits the hit by itself and the shadow loop in step 6 needs no "does this count?" answer.
    Both tutorials set it; the Khronos shadow chapter adds non-opaque handling only for transparency.
  - Compaction copies a built structure into a smaller buffer. NVIDIA recommends it when memory is
    tight and claims 20 to 50% savings without measurements. It waits until memory is a concern.
- **d. One scratch buffer per build, destroyed after the build.** The same pattern as the staging
  buffer in `Buffer`'s upload constructor: `SubmitAndWait` has returned, so the GPU no longer reads
  it. The scratch address must be a multiple of `minAccelerationStructureScratchOffsetAlignment`
  (128 on this machine's RTX 4060, 256 on its AMD Radeon 610M), and a fresh buffer's address is not
  guaranteed to be. So the buffer is the scratch size plus the alignment, and the address is rounded
  up. NVIDIA shares one scratch buffer across batched builds; without batching there is nothing to
  share.

**Sources:** Khronos tutorial, "Acceleration structures"
(https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/02_Acceleration_structures.html) and
"Ray query shadows"; NVIDIA tutorial, "Acceleration structures"
(https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/concepts/acceleration-structures/); Arm's
ray traversal guide on opaque and non-opaque candidates.

Seam: ambient occlusion and reflections trace into the same structures.

## 5. TLAS each frame, split into fill and build

**The problem:** the TLAS is the scene: one instance per drawn object, each saying "this BLAS, at this
transform". Objects move, so it is rebuilt every frame from the `DrawList`, which already holds every
drawn object. The fill (CPU writes the instances) and the build (GPU builds the tree from them) are
separate, so a later compute fill does not change the build. Raster and the TLAS both read
`DrawItem.transform`, so the two copies of each matrix never disagree. Neither tutorial covers frames
in flight: Khronos uses one TLAS built in a one-time command buffer that waits, which stalls the CPU
each frame, and NVIDIA leaves the per-frame side out.

**Decisions**

- **a. One TLAS, instance buffer and scratch buffer per frame slot.** The same pattern as the per-slot
  uniform buffers in `Descriptors`: each slot's set 0 points at its own TLAS. The CPU writes one
  slot's instances while the GPU may read the other's, and the slot's fence already guarantees
  safety. The cost is two TLAS copies, small next to the meshes.
- **b. A full rebuild every frame, with `PREFER_FAST_BUILD`.** The build mode is rebuild, not update; the
  flag follows from it, because a tree rebuilt every frame should build fast. Both tutorials prefer
  update mode (Khronos with `ALLOW_UPDATE`), but an update needs the same instance count as the last
  build, scenes change counts (`PhysicsScene`'s R respawns, scene switches), and per-slot TLAS
  complicates it. If the "TLAS build" zone shows a real cost, update mode is the next move.
- **c. The instance buffer is CPU-writable memory with spare room, and grows when full.** Host visible
  and coherent, written by the CPU each frame and read by the GPU's build, with
  `SHADER_DEVICE_ADDRESS` and `ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY` usage (step 3's `Buffer`
  sets the allocate flag). The TLAS storage is sized for a maximum instance count from
  `vkGetAccelerationStructureBuildSizesKHR`. When the `DrawList` passes that maximum, the slot
  recreates its instance buffer, TLAS and scratch buffer at double the count and points its set 0 at
  the new TLAS. That is safe because the slot's fence has passed. Most frames resize nothing.
- **d. The build is recorded in the frame's own command buffer, before rendering.** In
  `RecordCommandBuffer`, before `vkCmdBeginRendering`, inside `PROFILE_GPU_SCOPE(..., "TLAS build")`.
  One barrier follows it: from `ACCELERATION_STRUCTURE_BUILD` with `ACCELERATION_STRUCTURE_WRITE` to
  `FRAGMENT_SHADER` with `ACCELERATION_STRUCTURE_READ`, the same pair as Khronos's post-build barrier.
  Khronos's pre-build barrier guards its single TLAS against the last frame's reads; per-slot TLAS does
  not need it. The slot's scratch buffer is reused every frame, as in Khronos, with step 4's alignment
  rounding.
  - Rejected: Khronos's one-time command buffer and wait, because the CPU stalls every frame. A
    separate submit with a semaphore, because it adds sync objects for no gain. A separate compute
    queue, because it is complex and only worth it if the zone says so.
  - The cost: the build sits on the frame's critical path, and fragment shaders wait for it. The
    barrier blocks only the fragment stage, so vertex work can start early. The zones time the build
    and the draws but not the wait between them; if both look cheap but the frame got slower, look
    there.
- **e. The fill turns each `DrawItem` into one instance.** Every drawn object becomes one
  `VkAccelerationStructureInstanceKHR`, so the instance array holds the whole drawn scene. 1,000 cubes
  in `Scene0` are 1,000 instances pointing at one cube BLAS.
  - Transform: the column-major `Matrix4` becomes a 3x4 row-major `VkTransformMatrixKHR`; row `r`,
    column `c` comes from `m[c * 4 + r]`.
  - BLAS address: the stored `mesh.blas` address, through `ResourceStore::GetMesh(item.mesh)`.
  - Mask `0xFF` and custom index `0`, as in NVIDIA. Flags `0`: NVIDIA's cull-disable flag matters only
    when a ray asks for face culling, and the shadow rays do not.
  - An empty `DrawList` still builds, with zero instances. Believed valid but not confirmed;
    validation checks it in the build.

**Sources:** Khronos tutorial, "TLAS animation"
(https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/04_TLAS_animation.html); NVIDIA
tutorial, "Acceleration structures"
(https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/concepts/acceleration-structures/).

Seam: a compute shader can take over the fill later, once transforms live in a GPU scene buffer, with
no change to the build.

## 6. Shadows in the shader

**The problem:** `phong.frag` loops over up to 4 point lights and adds diffuse and specular for each,
plus ambient once. Each light's two terms need a visibility factor from a ray toward the light. None
of the three sources (the Khronos Vulkan-Samples `ray_queries` sample, the Khronos tutorial, NVIDIA's
ray query sample) covers GPUs without ray query, so decisions a to c lean on this engine. The Khronos
sample is the closest match: GLSL, a fragment shader, and a point light.

**Decisions**

- **a. The TLAS is set 0, binding 2, fragment stage.** Set 0 already holds the per-slot camera
  (binding 0) and lights (binding 1), and step 5 made the TLAS per slot. On a GPU without ray query,
  set 0 has no binding 2, because the descriptor type needs the extension. Binding numbers may skip,
  so a later binding 3 is 3 on every GPU. The Khronos sample uses set 0, binding 0.
- **b. One shader source, compiled twice.** `phong.frag` wraps the ray query code in an `#ifdef`, and
  CMake compiles it once plain and once with the define, to two SPIR-V files. A module that declares
  the ray query capability cannot load on a GPU without the feature, so a runtime branch cannot
  replace the second file. The ray query variant needs `#version 460`, as in the Khronos sample.
  - Rejected: a separate `phongWithRay.frag`, which copies the lighting and lets the two drift.
    Two files sharing the lighting through `#include`, because the CMake rule lists only the shader
    itself as a dependency, so editing the shared file would not recompile.
  - The cost: a second CMake rule for this file, with its own output name.
- **c. The switch picks the pipeline.** On supported GPUs the renderer also builds a shadow mesh
  pipeline from the second SPIR-V file. The switch decides which mesh pipeline is bound, so the shader
  has no on/off branch. `Application` flips it with an app key and calls the renderer, such as
  `SetShadows(bool)`; on unsupported GPUs it stays off. Shadow maps later become one more pipeline
  behind the same switch.
- **d. The ray, after the Khronos sample.** Origin: the fragment's world position (step 7 adds the
  offset). Direction: `normalize(lightPosition - position)`, with `tMax` the distance to the light, so
  the ray ends at the light and objects behind it cast no shadow. `tMin` is a small world distance;
  step 7 sets it. Flags `gl_RayFlagsTerminateOnFirstHitEXT`, cull mask `0xFF`, and one
  `rayQueryProceedEXT` call with no loop: with that flag and opaque geometry, one call is enough. A
  committed intersection means shadow.
  - Changed from the Khronos sample, found in step 7's design: the sample leaves the direction
    unnormalized with `tMax = 1.0` and `tMin = 0.01`. Then `t` is a fraction of the distance to the
    light, so `tMin` skips 1 cm for a light 1 m away but 10 cm for one 10 m away, enough to miss a
    real blocker. A normalized direction makes every number a world distance at any range.
- **e. Every light casts a shadow.** One ray per light per pixel, up to 4. Step 7 measures the cost.
- **f. The shadow darkens that light's diffuse and specular; ambient stays.** `shadowFactor` returns 0 or
  1 per light, and both terms are multiplied by it. Ambient is the light that reaches every surface.
  The Khronos sample and tutorial multiply the whole colour by 0.2, which works only for one light.
- **g. `glslc --target-env=vulkan1.3` for every shader.** `VK_KHR_ray_query` needs `VK_KHR_spirv_1_4` or
  Vulkan 1.2, and glslc emits SPIR-V 1.0 by default. The engine already requires 1.3, so the plain
  shaders lose nothing.

**Sources:** the Khronos Vulkan-Samples `ray_queries` shader
(https://raw.githubusercontent.com/KhronosGroup/Vulkan-Samples/main/shaders/ray_queries/glsl/ray_shadow.frag),
the Khronos tutorial "Ray query shadows"
(https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/03_Ray_query_shadows.html), NVIDIA's
"16 ray query" sample, and the `VK_KHR_ray_query` reference page.

Seam: shadow maps become a second body for `shadowFactor`, and the switch becomes the method choice.

## 7. Fix artifacts and measure

**Decided**

- **a. The scene: `Scene1` with a floor cube.** `Scene1` has done its job as a lighting test and becomes
  the lights and shadows scene, so its lights and objects may move freely for testing. Mario and the
  cubes already shadow themselves and each other (the cap onto the face, Mario onto a cube), and the
  five lighting schemes on keys 1 to 5 give several light directions. The floor adds a flat surface
  that shows each shadow's whole shape and shows peter-panning, a gap between an object's base and
  where its shadow starts. Scale is uniform (STATE.md, architecture rule 10), so the floor is the
  existing cube mesh at a large uniform scale, sunk so only its top face shows: scale 20 at y = -10.5
  puts the top face at y = -0.5, level with the bottoms of the two cubes. Mario's feet may sit at
  another height; check once it renders. It has no collider. The bonus below flattens it.
  Self-shadowing on Mario is the best acne test, and the floor is the best peter-panning test.
  - The floor's material is a plain grey texture, one new small PNG. Shadows read best on a flat
    neutral colour, and the skull texture would stretch across the face and hide their shapes. A
    colour in `MaterialDesc` (ROADMAP.md, Miscellaneous, row 1) is the cleaner fix and out of scope.
- **b. The self-shadowing fix: a fixed offset along the normal, a small `tMin`, and no ray when
  `diffuse <= 0`.** The ray starts a little off the surface along the interpolated normal, which is
  close to the geometric normal on these meshes. A side facing away from the light is already dark,
  so its ray is skipped: that saves rays and removes acne there. Ray Tracing Gems, chapter 6
  (Wächter and Binder, NVIDIA) argues for offsetting along the normal; the Khronos sample uses `tMin`
  alone.
  - Rejected for now: that chapter's robust offset, which NVIDIA's ray query sample uses as
    `pointOffset`. It moves the point by a few floating-point steps sized to the rounding error of a
    ray hit computed from the triangle. A rasterized fragment position carries the rasterizer's
    interpolation error instead, believed larger (not confirmed), so the tiny offset could leave
    acne. It is the right tool once rays start from ray hits: reflections, or a second AO bounce.
- **c. The offset is one named constant in the shader, starting at 0.01 world units.** The cube is 1
  unit. Screenshots per light scheme from `run-game.ps1` tune it: lower it until acne appears, then
  back off, checking peter-panning on the floor and the cap-to-face contact. No source gives a
  universal number, because it depends on scene scale.
- **d. The low-poly edge artifact is accepted until it is seen.** The "shadow terminator": interpolated
  normals let the shadow edge look jagged where light grazes a low-poly surface. The cube has hard
  edges; Mario is smooth, so it shows there if anywhere. Ray Tracing Gems II, chapter 4 ("Hacking the
  Shadow Terminator", Hanika) and Disney's "Taming the Shadow Terminator" cover fixes; neither was
  read in full.
- **e. The measuring protocol.**
  - Scenes: `Scene0` (1,000 instances, the TLAS and ray count test) and `Scene1` with a fixed light
    scheme and camera.
  - Runs: Release, three warm runs per case, the mean reported. The ~8% noise floor is a CPU finding;
    the first job is three shadows-off runs to find the GPU's.
  - Present mode: cap off, IMMEDIATE or MAILBOX. Under FIFO the GPU idles between frames and may
    lower its clocks, which would inflate GPU times; an assumption, checked by the first runs.
  - Cases: shadows off against on, comparing the `"Mesh pass"` and `"TLAS build"` zones.
  - Record: a "What the build checked" list in this section.

**Sources:** Ray Tracing Gems, chapter 6
(https://research.nvidia.com/publication/2019-03_fast-and-robust-method-avoiding-self-intersection);
NVIDIA's blog "Solving self-intersection artifacts in DirectX Raytracing"; Ray Tracing Gems II
contents; Disney's "Taming the Shadow Terminator"; the Khronos `ray_queries` shader.

**Bonus, at the end of this step: non-uniform scale**

Independent of the shadows. The TLAS already takes any 3x4 transform, so shadows need no change.
Decided: round colliders need uniform scale. The rest is a list of what changes; it gets a short
design pass before the bonus is built:

- **Data:** `Transform.scale` goes from `float` to `MATH::Vec3` (`core/Components.h`), and the model
  matrix build passes the three components to `MMath::scale`. Every `.scale = 1.0f` in the scenes
  changes.
- **Normals:** `FrameLoop.cpp` builds the normal matrix as the model's plain 3x3. It becomes the
  inverse transpose of that 3x3, one 3x3 inverse per draw on the CPU. The push constant already holds a
  3x3 (`Pipeline.h`).
- **Physics, per axis:** colliders are sized at scale 1 and multiplied by `Transform.scale`. The pick
  ray's move into local space (`Ray.cpp`), the contact point and the box inertia tensor
  (`RigidBody.cpp`) become per axis and stay correct. `PickSystem`'s near-zero scale checks change too.
- **Round colliders need uniform scale.** A sphere, capsule or cylinder scaled unevenly is a different
  shape, and its ray test and inertia tensor assume the real one. They read one component, and a debug
  assert checks that x, y and z are equal.
- **Docs:** STATE.md, architecture rule 10, names the uniform-scale dependency and changes with it.
- **The floor:** the floor cube from decision a becomes a flat box, such as (20, 0.5, 20). A wrong normal matrix
  lights a flattened box wrong, so the floor also tests the change.

---

## Later, outside this plan

Shadow maps, the GPU scene buffer and the compute fill of the TLAS are rows in ROADMAP.md, Rendering,
"After ray query shadows". Each waits for a real caller or a measurement.

Ambient occlusion is not on the roadmap. It needs only the TLAS, so it fits any time after step 6.
Interpolation with `Alpha()` is ROADMAP.md, Physics, "Fixed step additions", row 1, and is independent
of this plan.

## Reading

- Khronos tutorial, ray tracing course: acceleration structures, ray query shadows, TLAS animation.
  https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/02_Acceleration_structures.html
- Sascha Willems' Vulkan examples, `rayquery`: ray query shadows in GLSL.
- NVIDIA `vk_raytracing_tutorial_KHR`: the full BLAS and TLAS build.
  https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/
- Vulkan spec, acceleration structures chapter, and the `GL_EXT_ray_query` spec, for exact details.
