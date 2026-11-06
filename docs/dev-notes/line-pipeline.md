# Line pipeline: a second pipeline for debug lines

Designed 2026-10-22, built 2026-10-23, as step 2 of the plan that ended on 2026-11-03.

## Sources

- The Vulkan tutorial's physics debug drawer, compared below.
- A small search on 2026-10-22: the Vulkan tutorial, PlayCanvas, Unreal's line batcher and Box2D's
  debug draw all rebuild debug lines every frame. The search also warned against creating a new
  vertex buffer per frame.
- D3D12 and sokol, for the rejected name `PipelineDesc`.

## What it does

A second graphics pipeline draws coloured line segments. Every scene shows world axes (red X, green
Y, blue Z) and a ground grid. G hides and shows them. The lines test against the depth the meshes
wrote, and do not write depth themselves.

## `PipelineSettings`

- `Pipeline` keeps one class and one `CreateGraphicsPipeline`. The values that differ between
  pipelines move into a struct, `PipelineSettings`. Every other line of the function stays as it
  is: the viewport and scissor, one sample, fill mode, line width 1, the depth test on with `LESS`,
  blending off, the formats from the swapchain.
- The struct groups its fields by the section of `CreateGraphicsPipeline` that reads them, with the
  same names as the section headers: `vertexInput`, `inputAssembly`, `vertexShader`,
  `fragmentShader`, `rasterizer`, `depthTest`, and `layout`. So a field says which stage it changes.
- Each field defaults to the mesh pipeline's value, so a filled copy names only what differs. It is
  filled with nested designated initializers, which must follow the declaration order. The groups
  follow the pipeline stages, so that order is the natural one.
- A setting moves into the struct only when a real pipeline needs a different value. Blending stays
  fixed and off until a pipeline needs it.
- The struct owns its values in fixed arrays, not behind a `std::span`. A span to a local array
  inside the function that fills the struct would point at dead memory once that function returns.
  Vulkan guarantees at least 4 bound sets, so 4 slots are enough.
- The filled copies come from functions in `include/vulkan/PipelineSettings.h`:
  `MeshPipelineSettings` and `LinePipelineSettings`. They are functions, not constants, because the
  set layouts exist only once Vulkan has made them. The line vertex's Vulkan layout lives in the
  same file, because the line vertex sits in core and may not hold Vulkan types.
- `VulkanRenderer` holds the finished pipelines, `meshPipeline` and `linePipeline`, and stores no
  settings. Each is built in the constructor's initializer list from a temporary settings struct.
  `Pipeline` cannot be default built, copied or moved, so the initializer list is the only place.
- Scenes and `Application` never see the settings. The data type picks the pipeline: the `DrawList`
  draws with the mesh pipeline, the debug lines with the line pipeline.
- A later pipeline, such as a skybox, adds one settings function, one member, its shaders and its
  own record section in `FrameLoop`. It may add one field, such as `depthTest.compareOp`.

## `LineVertex`, `DebugLines` and the helpers

- They sit in `include/core/debug` and `src/core/debug`, so scenes and `Application` fill lines
  without any Vulkan header. Debug code sits in a `debug` subfolder of each layer, `core/debug` and
  `vulkan/debug`. Layer first, so the path keeps showing the layer rule.
- `LineVertex` is a position and a colour: 24 bytes, held by a `static_assert`.
- `DebugLines` is a `std::pmr::vector<LineVertex>` in the frame arena, two entries per line. It holds
  vertices, not lines, so it matches the GPU's layout byte for byte and the renderer copies it with
  one `memcpy`.
- The rule for later: a new data shape gets its own list and pass; a new look for the same shape
  gets a pipeline on the material.
- Free helpers only push vertices. `AddLine` writes two vertices with one colour, and every other
  helper is built on it. `AddAxes` draws red X, green Y and blue Z from the origin. `AddGrid` draws
  a square on y = 0.
- The axes lie on the grid's two centre lines, at the same depth. Lines do not write depth, so the
  line drawn last wins the pixel: the grid goes in first, the axes after.
- Colours are floats. Packed bytes would save a third of the memory and the copy, but need a packing
  function. Even 65,536 vertices are 1.5 MB per frame slot as floats. Packing stays a local change,
  made only if a measurement asks for it.
- `DrawItem` and `DrawList` moved from `core/Scene.h` to `include/core/DrawList.h`. Before, the
  renderer saw the whole `Scene` interface when it needed only a draw item. This matches `Camera.h`
  and `Light.h`, which both sides already share.
- Both lists hold facts, not commands: "this mesh, this material, here" and "a segment from A to B in
  this colour". Only the renderer turns them into Vulkan commands.

## The shaders

- `debug.vert` takes the position and the colour at locations 0 and 1. They must match the
  `LineVertex` attributes. It uses the same camera uniform block as `phong.vert`, copied whole so
  the two shaders compare easily, and writes `projection * view * position`.
- No push constant block, because the line pipeline's layout has no range; declaring one is a
  validation error. No model matrix, because the helpers write world space.
- `debug.frag` writes the colour with alpha 1. No lighting, no texture, no set 1.
- The GPU blends the colour between a line's two vertices, so two end colours give a fade for free.
- Descriptors: nothing new. The line pipeline reuses set 0, and the shaders ignore binding 1, the
  lights, which Vulkan allows. The vertices reach the GPU through `vkCmdBindVertexBuffers`.
- What a mistake looks like: "input location not consumed" or "not written" means a location
  mismatch; a push constant error means the shader declares a block; nothing on screen with no
  error usually means the camera block does not match `CameraUniform`.

## `LineBuffers`

- The GPU reads only Vulkan buffers, so the vertices are copied into one each frame. A mesh is
  copied once at load; the lines change every frame.
- One buffer per frame slot, because the GPU may still draw frame N from one buffer while the CPU
  writes frame N+1. The camera has one buffer per slot for the same reason.
- The class sits in `include/vulkan/debug/LineBuffers.h`. Each buffer is a vertex buffer in
  host-visible, host-coherent memory: the CPU writes it directly, and the GPU sees the writes with
  no flush call.
- `Write` throws when the list holds more than `kMaxLineVertices`, naming the count. The check is
  required: `Buffer::Write` does not compare the data size with the buffer size, so too many lines
  would write past the buffer's memory. An empty list returns at once.
- `VulkanRenderer` owns it, and `FrameLoop` gets a reference. `DrawFrame` writes it right after the
  fence wait, which proves the GPU has finished with this slot's buffer.
- Capacity: 65,536 vertices, 32,768 lines, 1.5 MB per slot and 3 MB in all. 1,000 box colliders are
  24,000 vertices; 1,000 sphere colliders at 3 circles of 32 segments are 192,000 and would throw. A
  scene that needs more raises the constant.
- Lines are rebuilt every frame, not stored. Storing unit shapes on the GPU and drawing each
  collider with a matrix would cost a draw call and a push constant per collider, plus a second
  path for loose lines, and debug shapes change size, colour and position anyway. Caching unit
  shapes with instancing is the upgrade if a profile ever asks for it.
- Debug draw stays in every build for now. The physics proofs need the lines.

**Compared with the Vulkan tutorial's debug drawer.** The same collect, upload once, one draw call,
and the same vertex. It keeps one 36-byte entry per line and splits it on upload; ours keeps one
24-byte entry per vertex, the colour stored twice, so the upload is one copy. It uses one buffer for
all frames and no capacity check, which risks overwriting lines the GPU still draws and writing past
the buffer. It turns the depth test off; ours tests depth and does not write it.

## The line pass

- `DrawFrame` keeps its six steps: wait, write, acquire, record, submit, present. The write step
  gains the line buffer, next to the camera and the lights, and is now "Write this slot's buffers".
- `RecordCommandBuffer` keeps its shared sections: begin, the image barriers, begin rendering, the
  viewport and scissor, and finish. Only the middle splits into `RecordMeshPass` and
  `RecordLinePass`, so a pass does not repeat the begin and finish.
- `RecordLinePass` returns when there are no vertices. Otherwise it binds the line pipeline, set 0,
  this slot's buffer, and makes one draw.
- Set 0 is bound again because two pipeline layouts with different push constant ranges are not
  "compatible" in Vulkan's sense. The bind made under the mesh layout no longer counts under the
  line layout, and the validation layer reports an error.
- The viewport and scissor are set once, before the passes. Both pipelines declare them dynamic, so
  they stay valid across the pipeline switch.
- The pipelines reach `FrameLoop` as one struct of references, `FramePipelines`. A third pipeline
  then adds a field and changes no signatures.
- Order: meshes, then lines. A skybox pass would sit between them.

## Wiring and the toggle

- The axes and the grid are an application-wide debug tool, not a scene's. `Application` adds them,
  so every scene shows them and no scene repeats the code.
- `Application` holds `showWorldReference`. `HandleEvent` flips it on G, key-down only and ignoring
  key repeat, so holding the key does not flicker it. The scene never sees G.
- Both lists take their memory from the frame arena, and the same `Reset` clears both.
- Step 2 gave `Scene` a `MakeDebugLines` with an empty default. Step 3 removed it again: its loops
  became Extract systems. See systems-and-phases.md.

## Rejected

- A copy of `Pipeline` per pipeline: about 250 lines each, and a fix in one misses the others.
- A base class with virtual hooks: a constructor cannot call a subclass's override, and the
  pipeline is built in the constructor.
- A builder with chained setters: more code than a struct, for two pipelines.
- More constructor parameters: about 10 positional arguments, and two bools side by side are easy to
  swap without a compiler error.
- The scene picking the settings: the struct holds Vulkan types, so the layer rule forbids it.
- Group names copied from Vulkan structs (`vertShaderStageInfo`): those structs hold many more
  fields than the group does.
- A `Line { from, to, color }` list: it reads better, but the renderer would expand each line into
  two vertices in a loop. `AddLine` gives the readable form instead.
- The name `DebugDrawList`: it suggests other shapes or text. The list holds only lines.
- One `RenderData.h` holding the camera, the lighting and both lists: a grab-bag file that every
  change touches.
- Reusing the Phong shaders: they expect a normal, a UV, a model matrix and a material.
- One shader with an `if` for lines: each pipeline still needs its own vertex layout.
- A colour per draw instead of per vertex: one draw per colour, where the point is one draw.
- The buffers in `Descriptors`: that class holds descriptor sets, and vertex buffers use none.
- The buffers in `FrameLoop`: it already does a lot, and owns no buffers.
- GPU-only memory with a staging copy each frame: a copy command and a barrier per frame for a few
  KB.
- One big buffer with an offset per slot: works the same, with offsets to get wrong.
- Growing the buffer when it fills: the GPU may still read the old one, so freeing it needs a
  deferred delete.
- Keeping the buffer mapped all the time: it saves two calls per frame, not measured, and
  `Buffer::Write` already exists.
- The two pipelines as two parameters: every new pipeline would change three signatures.
- The same push constant range on the line pipeline, only to skip the set 0 rebind: it declares a
  range the line shaders never use, to save one call.
- The full six sections inside each pass: the begin, barriers and finish happen once per frame, not
  once per pipeline.
- Each scene filling the axes and grid itself, as the outline had it.

## What the build found

- The line pipeline was built together with the shaders, so the validation layer checked the shaders
  against the vertex layout and set 0 before anything drew.
- The grid has half size 40 and the axes are 25 m, chosen by Vakarian on screen. Grey 0.3 shows as
  about 0.58, because the swapchain is `B8G8R8A8_SRGB` and the stored value is converted from
  linear.
- `.gitignore` listed `Debug/` unanchored, and git on Windows ignores case, so it hid every `debug`
  folder. The build-output patterns now start with `/`.
- The arena never frees a vector's old blocks, so the first build of the lines grew through 16 sizes
  (1,426 vertices, 34,224 bytes) to hold 330. `Application` keeps `lastDebugLineCount` and reserves
  it each frame, so the list follows what is drawn, the toggle included. The count starts at 512
  (12 KB), so the first frame reserves too. Without the seed, that first frame's growth set the high
  water mark on its own.

## Measured

- **`Record`,** Release, Scene0, 10 s runs alternated with a build of the commit that added `PipelineSettings`, the first run
  of each build dropped as cold: 0.131 ms before (0.133, 0.130, 0.129) and 0.134 ms after (0.146,
  0.125, 0.130), 2% apart, inside the 8% noise floor. A second set an hour later, with the seeded
  reserve, ran slower on both builds: 0.150 ms before (0.153, 0.155, 0.137, 0.155) and 0.148 ms
  after (0.157, 0.151, 0.153, 0.129).
- **Frame arena high water in Scene0:** 72,000 bytes before, 106,288 with the unseeded reserve,
  84,288 with the seed (the draw list plus 512 vertices of 24 bytes).
- **20 switches,** Debug, F1 and F2 in turn, G pressed once on the way: 21 ECS reports, 0 leaked
  blocks, no validation output, 0 requests fell to the heap, frame high water 79,952 bytes, peak
  live bytes 592,671 (72 more than after step 1b).
