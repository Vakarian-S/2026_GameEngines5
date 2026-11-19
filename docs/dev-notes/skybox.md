# Skybox

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed by 2026-11-16 and built 2026-11-16 to 2026-11-17, as all seven steps of the skybox plan, in
one commit (`7f3b591`). It came from the ROADMAP.md block "Skybox", which came from the skybox and
ImGui prototype of 2026-11-14 (branch `prototype/ui`). The ROADMAP.md block had 8 rows; the plan
merged row 4, the descriptor set, into step 4, because the design moved the cube map into set 0.
"Step N" below means a step of the skybox plan, which ended on 2026-11-17; README.md maps each one to
its note. Each section below is the step's whole plan section.

The plan's goal: before it, the space behind the meshes showed a flat clear colour. A skybox paints
that space with a cube map, which turns with the camera and never moves with it.

**References.** Sascha Willems' `texturecubemap` example shows the Vulkan side of a cube map: the
`CUBE_COMPATIBLE` image, the 6 layers and the barriers. His blog post
[Vulkan tutorial on rendering a fullscreen quad without buffers](https://www.saschawillems.de/blog/2016/08/13/vulkan-tutorial-on-rendering-a-fullscreen-quad-without-buffers/)
shows the screen triangle. LearnOpenGL's "Cubemaps" chapter explains the idea in OpenGL.

**The design adds no `Skybox` class.** The image, the descriptor and the pipeline each live in their
own class, the same split the meshes use: `Texture`, the descriptor classes and `Pipeline`.

---

## Step 1: Pipeline settings for the sky

`Pipeline.cpp` hard-codes `VK_COMPARE_OP_LESS` and always sets one vertex binding. The sky needs
`LESS_OR_EQUAL`, and it reads no vertex buffer.

- **a. A `compare` field in `PipelineSettings::DepthTest`.** It is `VkCompareOp compare =
  VK_COMPARE_OP_LESS`, next to `write`, and `Pipeline.cpp` reads it in place of the hard-coded `LESS`.
  The default keeps the mesh value, so the mesh and line settings stay unchanged. The sky sets
  `LESS_OR_EQUAL`, because its depth of 1.0 equals the cleared depth, and `LESS` would throw away
  every sky pixel.
- **b. No vertex binding when there are no attributes.** The binding count becomes
  `attributeDescriptionCount == 0 ? 0 : 1`. A binding with no attributes reads nothing, so the
  attribute count already says whether a vertex buffer exists. Rejected: a separate `bool` field,
  which could disagree with the count.

**What the build checked:** x64 Debug and Release built. `run-tests.bat` passed in all four
configurations. `run-game.ps1` ran F1 to F4 on the RTX 4060: exit code 0, no `[WARNING]`, no
`[ERROR]`, 0 leaked blocks. The same run on the AMD 610M (`VK_LOADER_DRIVERS_SELECT=*amd*`) gave the
same result, plus the one expected loader warning for the hidden NVIDIA driver.

## Step 2: The sky shaders

- **a. One big triangle built from `gl_VertexIndex`.** The vertex shader picks one of three corners
  in clip space: (-1, -1), (3, -1) and (-1, 3). They cover the whole screen, and the GPU clips the rest.
  Each vertex writes `gl_Position = vec4(corner, 1.0, 1.0)`, so z equals w and the depth is exactly
  1.0. Reason: three vertices and no buffer. Rejected: Sascha's cube mesh, which works as well but
  needs a vertex buffer and a vertex binding. Both shade each sky pixel once.
- **b. The shader computes the direction with `inverse()`.** For each corner, the vertex shader runs
  `inverse(projection) * vec4(corner, 0.5, 1.0)`, divides by w, and turns the result with
  `transpose(mat3(view))`. The fragment shader normalizes the blended direction and samples the
  `samplerCube`. Reason: `inverse()` runs only 3 times a frame, and `CameraUniform` stays unchanged.
  Rejected: an inverse projection in `CameraUniform`, which every pipeline shares.
- **c. Unproject at z = 0.5, not 1.0.** The direction is the same, and an infinite far plane would put
  z = 1.0 at infinity, with w = 0.

**Things to watch out for:**

- Normalize only in the fragment shader. Normalizing each corner bends the sky near the screen edges.
- `transpose(mat3(view))` undoes only a rotation. The fly camera holds no scale, so it holds today.
- The inverse projection also undoes the projection's Y flip, so the sky needs no special case for it.
- Reversed-Z, if it ever comes, needs z = 0 and `GREATER_OR_EQUAL` here.

**What the build changed:** `skybox.frag` samples with `vec3(direction.x, direction.y, -direction.z)`.
Step 6, decision c explains why.

**What the build checked:** both shaders compile through the glslc glob in the build. The game run of
step 1 loaded the sky pipeline with no validation messages.

## Step 3: The cube map image

- **a. `CubeMap` is its own class, not a `Texture` with a layer count.** Every `Texture` caller wants a
  2D image for a material, and a cube flag would make each `Texture` function branch. `CubeMap`
  repeats the shape of `Texture`'s four functions (`CreateImage`, `UploadPixels`, `CreateImageView`,
  `CreateSampler`), with the cube changes: `VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT`, `arrayLayers = 6`,
  six copy regions with their own `baseArrayLayer`, a `VK_IMAGE_VIEW_TYPE_CUBE` view and a
  `CLAMP_TO_EDGE` sampler. Rejected: one class for both.
- **b. `RecordImageBarrier` gains `uint32_t layerCount = 1` as its last parameter.** Today it changes
  layer 0 only, so 5 faces would stay in the wrong layout. With the default, its 6 current calls stay
  unchanged, and `CubeMap` passes 6. Rejected: a whole `VkImageSubresourceRange` parameter.
- **c. One folder per sky, under `assets/skyboxes/`, with six fixed names.** For example
  `assets/skyboxes/space/` holds `px.png`, `nx.png`, `py.png`, `ny.png`, `pz.png` and `nz.png`. The
  constructor takes the folder path. One path gives one cache key, and the fixed names fix the layer
  order (+X, -X, +Y, -Y, +Z, -Z) in one place. Rejected: an array of six paths, which lets a caller
  swap two faces.
- **d. The constructor throws if the faces do not match.** Vulkan needs every face square and all six
  the same size. `CubeMap` checks each face after `stbi_load` and throws with the file name, as
  `Texture` throws when a load fails.

The faces load as `R8G8B8A8_SRGB`, like `Texture`, so the shader gets linear values. One staging
buffer holds all six faces.

**What the build checked:** a new test program, `GpuTests` (`tests/GpuTests.cpp`), x64 only. A hidden
SDL window gives `GPUContext` its surface, and the test sends `std::cerr` into a string, so any
validation `[WARNING]` or `[ERROR]` fails the check. 4 test cases, 9 checks, all passing in Debug and
Release:

- `tests/assets/cubemaps/good/`, six 4x4 faces: the constructor returns, the view and the sampler
  are set, and the validation layers print nothing.
- `wrong_size/` (`nz.png` is 8x8), `not_square/` (`py.png` is 4x8) and `missing_face/` (no `pz.png`):
  each throws, and the message names the file.

The test never samples the image, so a face copied into the wrong layer would still pass. Step 6's
screenshots cover that.

## Step 4: The cube map binding and the sky pipeline

- **a. `SkyboxPipelineSettings(frameSet)` in `PipelineSettings.h`.** It names only what differs from
  the mesh pipeline: the two shader files, zero vertex attributes, cull mode `NONE`, depth write off,
  compare `LESS_OR_EQUAL`, and the layout `{frameSet}`. `pushConstantSize` stays 0. Reason: it follows
  `LinePipelineSettings`.
- **b. `Pipeline skyPipeline` is a plain member of `VulkanRenderer`, not a `std::unique_ptr`.**
  `rayQueryMeshPipeline` is a pointer because some GPUs cannot build it. Every GPU can build the sky
  pipeline.
- **c. `skyPipeline` sits after `linePipeline`** in `VulkanRenderer`, because it needs the swapchain's
  formats and the set 0 layout.
- **d. `FramePipelines` gains `const Pipeline& skyPipeline`.** `FrameLoop` gets every pipeline through
  that struct. Rejected: one more `DrawFrame` parameter.
- **e. The cube map is set 0, binding 3, next to the TLAS at binding 2.** Lower set numbers hold data
  that changes less often, and one sky per scene changes less often than any material. `Descriptors`
  gains `WriteCubeMap(frameSlot, view, sampler)`, like `WriteTopLevelAccelerationStructure`, and
  binding 3 reaches only the fragment stage. Rejected: a `CubeMapDescriptors` class with its own
  layout and pool at set 1, which the design first chose and then dropped. The mesh shaders can
  sample the sky later, for reflections, with no new set.

Vulkan requires every binding that a bound pipeline reads to hold a valid descriptor. The mesh
shaders never read binding 3, and the sky pass is skipped when there is no sky, so an empty binding 3
is safe.

**What the build found:** the first run printed a validation warning: the pool had no room for a
`COMBINED_IMAGE_SAMPLER`. `poolInfo.poolSizeCount` still used the old `SupportsRayQuery() ? 2u : 1u`,
so the new pool size never counted. It now uses the running `poolSizeCount`, as the layout uses
`bindingCount`.

**What the build checked:** the step 1 run, after the pool fix. Both GPUs here support ray query, so
the layout without the TLAS (bindings 0, 1 and 3) has never run.

## Step 5: The sky pass

- **a. `FrameLoop::DrawFrame` takes `const CubeMap* sky`.** `VulkanRenderer` turns the scene's handle
  into a pointer through `resourceStore`, and an empty handle becomes `nullptr`. `FrameLoop` needs the
  view and the sampler for the write, and a pointer says "maybe none" in one value. Rejected: the
  handle plus a lookup inside `FrameLoop`.
- **b. `descriptors.WriteCubeMap(currentFrame, ...)` runs after the TLAS write in
  `FrameLoop::DrawFrame`,** only when `sky` is not null. By then, the slot's fence has passed, so no
  frame in flight reads the set while it changes.
- **c. A new private function, `RecordSkyPass(commandBuffer, pipeline)`.** It binds the sky pipeline,
  binds set 0 again, and calls `vkCmdDraw(commandBuffer, 3, 1, 0, 0)`. Set 0 must be bound again
  because the sky layout's push constant ranges differ from the mesh layout's.
- **d. It runs as a new section "6. Sky Pass", between the mesh pass and the line pass,** only when
  `sky` is not null. The line pass becomes section 7 and the finish section 8. Drawn after the
  meshes, the sky shades only the pixels they left at depth 1.0. The lines must come after the sky,
  because they write no depth and the sky would paint over them. The viewport and scissor stay
  valid, because they are dynamic.
- **e. No GPU timer zone for now.** The sky shades at most one full screen. A `GPUTimerName::SkyPass`
  costs one line if a measurement ever needs it.

**What the build found:** `RecordCommandBuffer` takes `bool drawSky`. The first version put the
`if (drawSky)` on the line pass, so the sky always drew and the debug lines vanished with no sky. It
did not show while `VulkanRenderer` held a test sky that always existed. It now guards
`RecordSkyPass`.

**What the build checked:** the step 7 run below covers it: the sky behind the meshes, the lines on
top, and the lines still drawn when a scheme has no sky.

## Step 6: The face images

- **a. A marked test sky first, in `assets/skyboxes/test/`.** Each face gets its own solid colour and
  its name in big letters, such as "+X" or "-Z". Claude makes the six PNGs with a script during the
  build. Only faces you can tell apart show a wrong facing. Rejected: testing with a real sky, where a
  swapped or mirrored face can look fine.
- **b. The check runs through `scripts/run-game.ps1`.** It turns the fly camera along +X, -X, +Z, -Z,
  straight up and straight down, and takes a screenshot each time. Each screenshot must show the right
  face name, readable and not mirrored. Up and down need extra care: their letters should sit upright
  when the camera faces -Z and then tilts.
- **c. A mirrored face is fixed in `skybox.frag`, as one flipped axis on the direction.** Every
  downloaded pack then works without edits. Most packs follow OpenGL's cube map convention. Rejected:
  renaming or flipping the files on disk for each pack.
- **d. Faces are PNG only.** A pack in JPG gets converted once when it is added, so `CubeMap` never
  guesses an extension.
- **e. Then one real sky, at 1024 × 1024 per face.** That is 6 × 4 MB = 24 MB of GPU memory (arithmetic,
  not a measurement). From memory, not checked: Humus's cube maps are free under CC BY 3.0, which asks
  for credit, and Poly Haven skies are CC0 but come as one panorama that needs a conversion to six
  faces. Check the license of the chosen pack before it goes in the repo.

**What the build changed:**

- **6a.** The test sky is six 512x512 faces in `assets/skyboxes/test/`, each with its own colour, a
  black border and its slot name. Nothing loads it any more.
- **6b.** The script run along six axes did not happen. Vakarian checked the facing by eye: every
  name reads correctly, `px` sits on the right, `py` overhead and `pz` straight ahead.
- **6c.** The flipped axis is z. Cube maps follow a left-handed convention, and this world is
  right-handed, so every face showed mirrored. Any one flipped axis removes the mirror, and each swaps
  one pair of faces. Flipping z keeps +X on the right and +Y up, and puts the pack's `pz` image ahead
  of a camera that looks down -Z. Sascha's `texturecubemap/skybox.vert` flips x and y instead: his
  world keeps Vulkan's Y-down, and x is his mirror fix.
- **6e.** The real skies are Vakarian's own sheets, not a downloaded pack. Each sheet is a 3x2 grid of
  2048x2048 squares (down, up, back on top; left, front, right below), and a script cut them into
  `px`…`nz`. Before the commit Vakarian renamed the folders: `night/`, `red_skies/` and `sunset/`. The
  script compared every edge pair that should touch: the side and top seams differ by 0 to 1.3 out of
  255, and edges that do not belong together by 11 to 43. The faces are 2048, not 1024, so each sky
  takes 6 × 16 MB = 96 MB of GPU memory (arithmetic, not a measurement). `night/` and `red_skies/`
  still hold their source sheet, `sky1.png`.

## Step 7: Who picks the sky

- **a. `ResourceStore` owns every `CubeMap`, behind a `CubeMapHandle`, cached by folder path.** Scenes
  can switch skies, and a scene that comes back to a sky gets the cached one. `ResourceStore` frees
  only at exit, so each loaded sky stays in GPU memory for the whole run. `CubeMapHandle` sits next to
  the other handles, so a scene never sees a Vulkan type (architecture rule 1).
- **b. `Scene` gets `virtual CubeMapHandle GetSky() const`.** It follows `GetLighting()`: one value per
  scene, not one per entity. The default returns an empty handle, and an empty handle means no sky, so
  the frame shows the clear colour. `Application` passes the handle to `VulkanRenderer::DrawFrame` as
  a fifth parameter. Rejected: a sky entity with a component in the draw list, which adds a query and
  a rule for two skies, for something that has no position and exists once.
- **c. `ResourceLoader` gains `CubeMapHandle CreateCubemap(const std::string& folderPath)`.** A scene
  loads its sky in `OnCreate`, as it loads meshes and textures, and keeps the handle for `GetSky()`.
  The design named it `LoadCubeMap`. The build named it after the other `Create…` functions.

`FrameLoop::DrawFrame` now takes 8 parameters. If a ninth ever comes, the per-frame values should go
into one struct.

**What the build changed:**

- `CubeMapHandle` defaults its index to `UINT32_MAX`, and that value means no sky. The other handles
  have no empty value.
- `ResourceStore::GetCubemap` returns `const CubeMap*`: `nullptr` for the empty handle, and an assert
  for any other index past the end of the list.
- Each Scene1 lighting scheme names a sky folder in a new `skybox` field, and `nullptr` means no sky.
  `OnCreate` loads each one into `skyboxSchemes`, and `GetSky()` returns the entry for
  `activeScheme`, so keys 1 to 4 switch the lighting and the sky together. Sunset shows
  `sunset`, Moonlight `night`, Backlight `red_skies` (its light is now a deep red to match), and RGB
  windows no sky.
- The cache key is the exact folder string, so `red_skies` and `red_skies/` would load twice.

**What the build checked:** x64 Debug and Release built. `run-tests.bat` passed in all four
configurations: on x64, `MemoryMonitorTests`, `EcsTests`, `ArenaTests`, `PhysicsTests` and
`GpuTests` (9 of 9); on Win32, the first four. One `run-game.ps1` run on the RTX 4060, Debug: Scene0,
G twice, F2 (Scene1), keys 1, 2, 3 and 4, G, key 3, G, F3, F4, F2 again, F1. Exit code 0, no
`[WARNING]`, no `[ERROR]`, no asserts, 0 leaked blocks, 0 world or frame arena requests fell to the
heap. The screenshots showed each scheme's sky behind the ruin and through its back window, the
clear colour on key 4 with the grid lines still drawn, and the clear colour in Scene0, F3 and F4. The
first Scene1 `OnCreate` took 6,405 ms with the three skies; the other five scene loads, the second
Scene1 included, took about 140 ms together, because the cache returned the loaded skies. The AMD
610M was not run for steps 5 to 7.
