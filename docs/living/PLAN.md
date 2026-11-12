# Plan

**Last updated:** 2026-11-11.

This plan adds hard shadows from ray queries. The fragment shader casts one ray from each surface point
toward each light and darkens that light when something blocks the ray. The course assignment asks for
shadows; ray query was chosen over shadow maps, and shadow maps stay a later fallback. No ROADMAP.md
block covers this; the plan came from the assignment and the design talk of 2026-11-11.

The plan builds only ray query shadows, but it keeps seams open for the later features listed at the
end: shadow maps, a GPU scene buffer, a compute fill of the TLAS, and ambient occlusion.

## Steps

| # | step | done when | stage |
|---|---|---|---|
| 1 | GPU timestamps around the mesh pass | The once-a-second stats line shows the mesh pass GPU time in ms | outline |
| 2 | Ray query support on the device | The device starts with the ray query extensions and `bufferDeviceAddress`, the new functions load, and validation stays clean | outline |
| 3 | Mesh buffers ready for acceleration structure builds | Vertex and index buffers carry the build flags, and `Buffer` returns its device address | outline |
| 4 | `AccelerationStructure` module and one BLAS per mesh | Each `Mesh` holds a BLAS built at load, and validation stays clean | outline |
| 5 | TLAS each frame, split into fill and build | The CPU fills instances from the `DrawList`, the build reads only the instance buffer, and the TLAS is ready before the mesh pass | outline |
| 6 | Shadows in the shader | The TLAS is bound in set 0, lighting calls only `shadowFactor`, a shadows on/off switch works, and a cube casts a hard shadow in `Scene1` | outline |
| 7 | Fix artifacts and measure | No self-shadowing acne, and shadows on/off GPU times are recorded | outline |

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

The mesh pass is `RecordMeshPass` in `FrameLoop.cpp`, once per frame. Timestamp queries write the GPU
time before and after it. The CPU reads a slot's results when that slot comes around again, so the read
never stalls. The times join the once-a-second `FrameTimeStats` line, never a print per frame.

Seam: every later choice gets measured with this, such as shadow maps against ray query.

## 2. Ray query support on the device

`GPUContext.cpp` checks the GPU for `VK_KHR_acceleration_structure`, `VK_KHR_ray_query` and
`VK_KHR_deferred_host_operations`, enables them, and turns on `bufferDeviceAddress` in the 1.2 features.
The acceleration structure calls are extension functions, loaded with `vkGetDeviceProcAddr`.

Seam: shadows, ambient occlusion and reflections all reuse this setup.

## 3. Mesh buffers ready for acceleration structure builds

`ResourceStore.cpp` creates vertex and index buffers with the vertex or index flag only. A BLAS build
reads them by device address, so they also need the device address flag and the build input flag.
`Buffer` gains a way to return its device address.

## 4. `AccelerationStructure` module and one BLAS per mesh

One class covers both kinds: the buffer that holds the structure, its handle, and its device address.
`ResourceStore` builds one BLAS per mesh at load, after the vertex and index buffers. The mesh cache
means one BLAS per mesh path, however many entities use it. The renderer owns the module; shadow code
only reads it.

Seam: ambient occlusion and reflections trace into the same structures.

## 5. TLAS each frame, split into fill and build

The fill turns each `DrawItem` into one `VkAccelerationStructureInstanceKHR`: its mesh's BLAS address,
its transform as a 3x4 row-major matrix, and a mask. The build reads only the instance buffer and does
not know who filled it. The TLAS is rebuilt each frame, before the mesh pass, with a barrier between.

Raster and the TLAS both read `DrawItem.transform`, so the two copies of each matrix never disagree.

Seam: a compute shader can take over the fill later, once transforms live in a GPU scene buffer, with
no change to the build.

Open: one TLAS per frame slot, or one TLAS and a wait.

## 6. Shadows in the shader

`Descriptors.cpp` adds an acceleration structure binding to set 0. `phong.frag` enables
`GL_EXT_ray_query`, and the lighting loop multiplies each light's diffuse and specular by
`shadowFactor`. A shadows on/off switch picks whether the factor is traced or always 1.

Seam: shadow maps become a second body for `shadowFactor`, and the switch becomes the method choice.

Open: whether `glslc` needs `--target-env=vulkan1.3` for ray query SPIR-V.

## 7. Fix artifacts and measure

The ray starts a small distance off the surface along the normal, so a surface does not shadow itself.
Step 1's timestamps record the mesh pass with shadows on and off, in `Scene0` and `Scene1`.

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
