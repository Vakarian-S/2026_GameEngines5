# Vulkan renderer

Moved from the semester plan on 2026-10-20.

---

## Two render backends

The render system must not call Vulkan or the Ubisoft API directly. It produces a **draw list**
— an array of `{ mesh, material, transform }` — and a backend consumes it. Swap backends and
nothing on the ECS side changes.

That also settles a question left open at the start of the design, about whether the rendering
API is retained or immediate. With two different APIs an intermediate form is needed either
way, so build that and let each backend do as it likes underneath.

---

## Rendering — what is known, and what is open

Surveyed 2026-09-28. This section used to be deliberately empty because a plan written before
reading the old code would be guesswork. Both codebases have now been read, so the guesswork is
gone and what remains are real choices. **Closed 2026-10-23: questions 1 to 4 are answered in
code, and question 5 moved to living/ROADMAP.md.**

### The Vulkan renderer — where it stands

Updated 2026-10-20. The renderer is written by hand here, class by class, with the old project
open as a reference. It is not a port: the Vulkan calls carry over, the structure does not.

Eight milestones, each ending in something that can be seen:

| # | builds | proof | status |
|---|---|---|---|
| 1 | `GPUContext` — instance, debug messenger, surface, GPU choice, device, queues | GPU name printed, no validation output on exit | done |
| 2 | `Swapchain` and `FrameLoop` | window clears to a colour and survives resize, minimize, maximize | done 2026-10-09 |
| 3 | `Pipeline` | one triangle, vertices hardcoded in the shader | done 2026-10-09 |
| 4 | `Buffer`, `Vertex`, `TemporaryCommandBuffers` | a cube from a vertex and index buffer | done 2026-10-12 |
| 5 | camera uniform buffer, per-object push constant, depth image, `Descriptors` | two cubes, one orbiting the other, with correct depth | done 2026-10-14 |
| 6 | real `DrawItem`, `ResourceStore`, `VulkanRenderer` on top | a thousand ECS entities drawn as cubes | done 2026-10-14 |
| 7 | `Texture`, and a second descriptor set for textures | two textures on alternating cubes | done 2026-10-15 |
| 8 | `ObjLoader`, Phong shaders, and materials | lit Mario with a material | done 2026-10-20 |

Milestone 6 is where the ECS and the renderer first met. `Main.cpp` now holds a `Window`, a
`VulkanRenderer` and a `Scene1`. Each frame the scene updates its world, fills a `DrawList` and
hands over its `Camera` and its lighting, and the renderer draws them. No Vulkan type reaches the
scene.

**How the classes are split: by lifetime.** Objects that are destroyed together share a class.

```
Window  ->  GPUContext  ->  Swapchain  ->  FrameLoop
            whole program   rebuilt on     per-frame work
                            every resize
```

Each takes the one before it by reference. `VulkanRenderer` holds them as members in this order,
so C++ destroys them in the reverse one, which is the order Vulkan requires. C++ builds and
destroys members by their order in the header, not by the order of the initializer list.
`GPUContext` knows nothing about the classes built on it.

More classes sit between those, and `VulkanRenderer` declares all of them before `FrameLoop`:

| class | owns | lives |
|---|---|---|
| `VulkanRenderer` | every class below, as members. It builds the Vulkan projection from the scene's `Camera` | whole program |
| `ResourceStore` | every mesh, texture and material, in GPU memory. A handle is a position in its list | whole program |
| `Texture` | one image, its memory, its view and its sampler. Loaded with `stb_image` and uploaded through a staging `Buffer` | as long as the store |
| `Mesh` | a vertex `Buffer`, an index `Buffer` and the index count | as long as the store |
| `Material` | the uniform `Buffer` with its shininess and specular strength, and the set that points at that buffer and at its texture | as long as the store |
| `Descriptors` | set 0, the camera and the lights: its layout, a pool, one set per frame slot, and one uniform buffer per binding per frame slot | whole program |
| `MaterialDescriptors` | set 1, the material: its layout and a pool of 64 sets. It allocates one set per material and keeps none | whole program |
| `TemporaryCommandBuffers` | a transient command pool. It lends a command buffer, runs it at once and waits | whole program |
| `Pipeline` | the pipeline and its layout | whole program. A resize does not rebuild it |
| `Buffer` | one `VkBuffer` and its memory | as long as the mesh it holds |
| `Vertex` | nothing. A struct that describes its own layout to the pipeline: position, normal, UV, 32 bytes | |
| `LoadObjFile` | nothing. A free function that reads an OBJ file into vertices and indices in CPU memory, with no Vulkan calls | |
| `RecordImageBarrier` | nothing. A free function that records one layout change. `FrameLoop` and `Texture` both call it | |

`Swapchain` also owns the depth image, its memory and its view. That is a convenience, not a
Vulkan fact: `vkCreateSwapchainKHR` makes colour images only. The depth image lives there because
it must match the swapchain's size, so both are rebuilt on the same resize. If a second image of
this kind arrives, such as an off-screen colour target, both move into their own class.

`FrameLoop` is declared last on purpose. C++ destroys it first, its destructor waits for the GPU
to finish, and only then are the meshes and the pipeline destroyed.

On the core side, `include/core` holds the types the scene and the renderer share. None of them
holds a Vulkan type:

| type | holds |
|---|---|
| `MeshHandle` | a `uint32_t` index into the renderer's store |
| `TextureHandle` | a `uint32_t` index into the renderer's store |
| `MaterialHandle` | a `uint32_t` index into the renderer's store |
| `MaterialDesc` | a `TextureHandle`, a shininess and a specular strength, which `Main` passes to `CreateMaterial` |
| `DrawItem` | a `MeshHandle`, a `MaterialHandle` and a model matrix: 72 bytes |
| `DrawList` | a `std::pmr::vector` of `DrawItem`, in memory the caller picks |
| `Camera` | the view matrix, the vertical field of view, and the near and far planes |
| `Light`, `SceneLighting` | a light's position and colour; an ambient colour and a span of lights |

**Decisions taken, and why:**

| decision | reason |
|---|---|
| **The C header, `vulkan.h`** | The old project and most references use it, and it shows every step. The Khronos tutorial uses `vulkan.hpp`, so its `vk::` names get translated |
| **Vulkan 1.3, features through the `Features2` chain** | The 1.0 feature struct cannot grow. Dynamic rendering and synchronization2 are only reachable through the chain |
| **Dynamic rendering, no render pass and no framebuffers** | Not faster on a desktop GPU. It removes two object types and the rebuild that keeps them in step on resize, and a pipeline then needs a format rather than a render pass. The cost is the layout changes per frame, written by hand |
| **synchronization2 for those layout changes** | `vkCmdPipelineBarrier2` matches the tutorial line for line. Acquire, submit and present stay on the original calls, where the old project is the reference |
| **One command buffer per frame slot, two slots** | The old code recorded every draw once per swapchain image and called `vkDeviceWaitIdle` each frame |
| **"Render finished" semaphore per swapchain image, the rest per frame slot** | The newer validation layer reports a per-slot one |
| **Resize sets a flag, and the rebuild happens at the top of the next frame** | Vulkan reports the mismatch in the middle of a frame. One fixed place to rebuild keeps the frame code simple |
| **Present mode prefers mailbox, and `Main.cpp` keeps its `SDL_Delay` limiter** | FIFO plus the limiter would wait twice. Switching to FIFO means deleting the limiter |
| **Viewport and scissor are dynamic state** | The old code stored the window size in the pipeline, so a resize needed a new pipeline. `FrameLoop` sets both every frame instead |
| **`DrawFrame` receives a camera, the lighting and a draw list** | `VulkanRenderer::DrawFrame(camera, lighting, drawList)` is the whole boundary. The scene never sees a pipeline, a buffer or a descriptor set |
| **A `DrawItem` is a mesh handle, a material handle and a model matrix** | A pointer to a `Buffer` would put a Vulkan type in the scene. A mesh name would cost a lookup per item per frame. A position alone cannot rotate or scale. Milestone 7 added a texture handle, and milestone 8 turned it into a material handle |
| **`MeshHandle` is a struct, not a bare `uint32_t`** | The compiler then refuses a plain number, or a later `MaterialHandle`, where a mesh is expected |
| **The scene owns a `Camera` without a projection or an aspect ratio** | The 0..1 depth range and the Y flip are Vulkan's, and the aspect ratio comes from the window. `VulkanRenderer` adds all three and fills a `CameraUniform`, the byte layout the shader reads. A second backend would build its own projection from the same `Camera` |
| **`ResourceStore` is Vulkan-side** | A stored mesh means buffers in GPU memory, which only Vulkan has. Another backend would get its own store behind the same handles. A core-side mesh loader that hands out handles waits until a scene must load its own files. Milestone 8 did not need one |
| **The store keeps each `Mesh` behind a `unique_ptr`** | A `Buffer` cannot move, so a `Mesh` cannot either, and a vector moves its elements when it grows. The pointer moves instead and the mesh stays put |
| **`Main` creates the meshes, textures and materials and passes their handles to the scene's constructor** | The cheapest way for a scene to receive a mesh or a material without seeing the store. Calling `CreateCube` twice makes two identical meshes; nothing prevents that yet |
| **`FrameLoop` reads the `DrawList` directly** | Translating it into a Vulkan-side list would copy every item for nothing. Vulkan code may include core types; core code may not include Vulkan ones |
| **Vertex and index buffers are bound only when the mesh changes** | The loop remembers the last mesh it bound. With one cube mesh, that is one bind per frame. With many meshes the list should be sorted by pipeline, then material, then mesh, so equal state sits together |
| **The clear colour is a constant inside `VulkanRenderer`** | `VkClearColorValue` is a Vulkan type, so the scene cannot pass it. The fading colour only proved that frames arrived, and moving cubes prove that now |
| **Buffers are filled through a staging buffer** | The CPU cannot write into the GPU's own memory. It writes into a buffer it can reach, and the GPU copies that across |
| **Load-time GPU work goes through `TemporaryCommandBuffers`, not `GPUContext`** | `GPUContext` is setup and ownership only. Submitting and waiting is work, so it has its own class, which `Texture` will also use |
| **That class has its own transient command pool** | The buffers are created before `FrameLoop`, so `FrameLoop`'s pool does not exist yet. The tutorial's text recommends a separate pool for short-lived command buffers |
| **32-bit indices** | 16-bit ones stop at 65,535 vertices, and the OBJ models of milestone 8 can pass that |
| **Back faces are culled, and counter-clockwise corners mark the front** | Clockwise until milestone 8, which switched to counter-clockwise because OBJ, glTF and Blender all wind that way. The cube was rewound to match |
| **The model matrix is a push constant** | It changes on every draw, and a push constant rides inside the command buffer. Vulkan guarantees only 128 bytes; this GPU allows 256. Measured in milestone 6: recording 1,000 draws takes 0.11 ms in Release, so it stays. At a million it takes 37 ms; the fix then is instancing, with the matrices in a storage buffer |
| **The camera is a uniform buffer in set 0** | It is shared by every draw in a frame and written once per frame |
| **One camera buffer and one set per frame slot** | The CPU writes the camera while the GPU may still read the previous frame's. The old code counted swapchain images because it recorded its command buffers once per image |
| **`Descriptors` started in milestone 5, not 7** | A uniform buffer cannot reach a shader without a descriptor set. Putting the camera in the push constant instead would have pushed descriptors and textures into one milestone |
| **`Descriptors` is driven by a table of uniform buffer bindings** | `kUniformBindings`: row i is binding i. A new per-frame uniform buffer is one row, one constant and one `Write` function. The table covers uniform buffers only. A texture changes per draw item, not per frame, so it lives in set 1, not in this table |
| **A new set is a new class, not a loop** | Bindings that change together share a set. A second set differs in count, contents and when it is bound, so a loop over sets would guess its shape. `Pipeline` takes a span of set layouts, entry i for set i. Milestone 7 added set 1 this way |
| **`PerspectiveVulkan`, then a Y flip** | The math library's `perspective` maps depth to −1..1, the OpenGL range. `PerspectiveVulkan` maps it to 0..1. Negating element `[5]` turns the picture the right way up, because Vulkan's +Y points down the screen |
| **Depth format `D32_SFLOAT`, checked at startup** | No stencil is needed. `vkGetPhysicalDeviceFormatProperties` confirms the GPU can use it as a depth attachment |
| **One depth image, shared by both frame slots** | The depth barrier's source masks wait for the previous frame's depth tests, so frames cannot clash. The cost is that two frames never test depth at the same time. One per slot would remove that, at about 3.7 MB each at 1280×720 |
| **Depth is cleared to 1.0 and not stored** | 1.0 is the far plane, so every real pixel passes `LESS`. Nothing reads depth after the frame, so `STORE_OP_DONT_CARE` |

**What the old code got wrong, and this one must not copy:**

- The scene holds pipelines, buffers and textures, and reaches the renderer with `dynamic_cast`.
- `SetPushConstant` builds a struct and then passes a different pointer with the struct's size,
  so Vulkan reads 128 bytes from a 64-byte matrix.
- `Render` throws on "out of date" instead of rebuilding.
- `RecreateSwapChain` allocates new command buffers without freeing the old ones.
- `DescriptorSetBuilder.cpp:153` points every descriptor set at buffer 0, so the per-image
  uniform buffers do nothing. It went unnoticed only because `vkDeviceWaitIdle` ran every frame.

**Temporary code removed in milestone 6:** the fading clear colour, the `Swapchain:` print, the
cube arrays and the camera in `Main.cpp`, and the `triangle.*` shader names, now `mesh.*`.

**Demo code that stays for now:** `Scene0`'s grid, its wrap box and its varied velocities exist
only to show the ECS driving the renderer. The doc comments at `Paths.h:10`, `Paths.h:26` and
`Pipeline.h:67` still name `triangle.vert.spv` as an example.

**Known smell:** `Swapchain` owns the depth image. It has a TODO, and the reason it stays is above.

**How a milestone is checked.** Build, run, close the window, and read the error stream. The
debug messenger prints every validation message as `Vulkan [SOURCE][SEVERITY]`. A clean run has
no `[WARNING]` and no `[ERROR]`, and an object left alive shows up as an error just before the
driver unloads.

### What milestone 6 measured

Measured 2026-10-14 on the RTX 4060 Laptop GPU, with `PROFILE_SCOPE("Record")` around
`RecordCommandBuffer`. `Record` is CPU time only: one push constant and one draw per item.

**At 1,000 entities**, one run each:

| build | Record avg | Record max | MakeDrawList avg |
|---|---|---|---|
| x64-Debug | 2.226 ms | 3.502 ms | 0.165 ms |
| x64-Release | 0.111 ms | 0.457 ms | 0.015 ms |

Debug runs about 20 times slower, mostly because the validation layers check every call. The
Release number is about a ninth of the 1 ms threshold set before measuring, so the model matrix
stays a push constant.

**At a million entities** (`kGridSide` 100), Release, three 20-second runs per setup in one
session, medians in ms. Four memory setups, to separate the arena from memory reuse:

| setup | frame | MakeDrawList | Record | Movement | OnCreate |
|---|---|---|---|---|---|
| A. arenas too small, everything falls to the heap | 53.7 | 13.3 | 37.3 | 1.85 | 171 |
| B. arenas sized right: world 128 MB, frame 80 MB | 46.6 | 7.3 | 37.1 | 1.79 | 178 |
| C. no arena, a new draw list every frame | 53.4 | 13.3 | 37.3 | 1.85 | 218 |
| D. no arena, one draw list cleared and reused | 47.1 | 7.3 | 36.9 | 2.17 | 206 |

What it says:

- **Record grows with the number of draws:** 0.11 ms at 1,000, 37 ms at a million. Recording
  alone fills a 16.7 ms frame at about 350,000 draws. Past that, instancing is the fix: one draw
  per mesh, with the matrices in a storage buffer.
- **The ECS is not the cost.** Moving a million entities takes about 2 ms. Building the list and
  recording it take the rest.
- **The arena is not faster than heap code that reuses its memory.** A matches C, and B matches D.
  The 6 ms saved in `MakeDrawList` comes from writing into the same 68 MB every frame instead of
  fresh memory. The likely cause is the OS mapping and zeroing about 16,600 new pages per frame;
  page faults were not counted.
- **An arena too small costs the same as no arena, not more.** It falls back to the heap, and the
  report and the Debug assert say so. Setup C is just as slow and says nothing.
- **`OnCreate` gains from reserving rows, not from the arena.** With no arena the world reserves
  nothing, so its columns double and copy 182 times. Any non-zero arena size makes each archetype
  reserve `maxEntities` rows, which is why A is fast although its arena overflowed.
- **Spawning moves each entity twice**, because `AddComponent` adds one component at a time:
  2,000,000 migrations and 12 MB copied for a million entities. A call that creates an entity
  with all its components would halve both.
- **Compare within one session.** An earlier single run showed Record at 47 ms. In this session
  it held at 37 ms in all four setups.
- Not explained yet: D's `Movement` is 17% slower than C's, though their worlds are set up alike.

The scene is back at 1,000 entities, with the 256 KB world and 128 KB frame arenas.

### Milestone 7 — the plan, and what it measured

Planned 2026-10-14. Done 2026-10-15, built as planned.

**Objective.** Cubes show an image instead of corner colours. Each `DrawItem` names a texture,
and two textures on alternating cubes prove that each item picks its own.

**How one set layout shows Texture A on one cube and Texture B on another.** "Set 1" names a
slot in the pipeline layout. A `VkDescriptorSet` is an object allocated from a pool, and many of
them can share one slot's layout. Each texture gets its own set object, written once at load
time. Before a draw, `vkCmdBindDescriptorSets` picks which object the shader reads as set 1. Set
0 already works this way: two objects with one layout, one per frame slot.

| slot | layout | objects | which one is bound |
|---|---|---|---|
| 0 | one uniform buffer, the camera | 2 | the current frame slot's |
| 1 | one combined image sampler | one per texture | the draw item's texture |

**Decisions taken, and why:**

| decision | reason |
|---|---|
| **Textures now, materials in milestone 8** | Today a material would hold one texture and nothing else. Phong gives it a reason: shininess and specular strength per surface. The switch renames `TextureHandle` to `MaterialHandle` and adds a uniform buffer binding to set 1. `FrameLoop`'s bind code stays the same |
| **`VulkanRenderer` keeps one `Pipeline`** | Milestones 7 and 8 each need one shader pair. When a second pair arrives, the renderer gets a pipeline list and the material stores an index into it, not the `DrawItem` |
| **A `DrawItem` gains a `TextureHandle`: 72 bytes** | A mesh that owns its texture could not be drawn with two textures. `FrameLoop` binds set 1 only when the texture changes, as it already does for meshes |
| **Set 1 is a new class, `TextureDescriptors`** | A texture changes per draw item and is written once, so it needs no copy per frame slot. `kUniformBindings` stays uniform-only. Set 1 has one binding and no table |
| **The pool has a fixed size** | Room for a fixed number of textures, such as 64. `CreateTexture` throws when it is full |
| **`ResourceStore::CreateTexture(path)` returns a `TextureHandle`** | It loads the image, makes the `Texture` and allocates its set. The scene sees only the handle; `Main` makes the call, as it does for the cube |
| **`ResourceStore` is built before `Pipeline`** | The pipeline layout needs the set 1 layout |
| **Each `Texture` owns its image, memory, view and sampler** | One sampler could serve every texture, because it holds settings and no data. One per texture costs nothing at this scale. It moves out when a second set of filter settings appears |
| **`stb_image`, asked for 4 channels** | This project has no SDL_image. Every file then arrives as RGBA, including a 3-channel JPG |
| **No vertical flip on load** | stb hands over the top row first, and Vulkan's V = 0 is the top of the image. The OBJ files of milestone 8 use OpenGL's V-up convention, so that flip happens in the OBJ loader |
| **The cube goes from 8 corners to 24** | Each face needs its own UVs, so faces cannot share corners. `Vertex` gains `uv` at location 2 and grows from 24 to 32 bytes |
| **The layout change moves out of `FrameLoop`** | `Texture` needs the same barrier code, so it becomes a shared free function |

**What we take from the old code:** the image create info, the copy from buffer to image, the
view, the sampler settings (linear filter, repeat, anisotropy), and the combined image sampler
write through `pImageInfo`.

**What we change:**

- The old code assumes 4 bytes per pixel from `IMG_Load`. A JPG loads as 3, so it reads the
  wrong bytes or runs past the end of the image.
- The old code never checks the load. Ours throws, with the file's name in the message.
- The old barriers use sync1 and `TOP_OF_PIPE`. Ours use `vkCmdPipelineBarrier2`, like `FrameLoop`.
- The old staging buffer is raw Vulkan calls. Ours is the `Buffer` class.

**How it is tested:**

1. Validation stays clean on build, run and close, with no image, sampler or pool left alive.
2. Before any texture, the fragment shader shows UVs as colour: U in red, V in green. Every face
   shows the same gradient, black in the top-left corner.
3. An image with a clear "up" looks upright and unmirrored on all four side faces.
4. Alternating cubes show two different textures.
5. A missing file throws with its name, and `Main` exits with code 1.
6. Resize still works.
7. `Record` in Release at 1,000 cubes, with the textures alternating: the worst case, one set
   bind per draw. Compared against milestone 6's 0.111 ms.

**Steps, each one builds and runs:**

1. Move the layout change out of `FrameLoop` into a shared function. Nothing on screen changes.
2. Add `stb_image` to `external/`. CMake copies `assets/textures/` next to the executable.
3. Write `Texture`. Build one in `VulkanRenderer` as a test, print its size, check validation.
4. Add UVs to `Vertex` and the cube, and show them as colour (test 2).
5. Write `TextureDescriptors`, `CreateTexture` and `TextureHandle`. `Pipeline` takes two layouts,
   and the members are reordered.
6. Add the texture to `DrawItem`, bind set 1 in `FrameLoop`, and sample it in the fragment
   shader (test 3).
7. `Scene0` alternates two textures (tests 4 to 7). Measure, then update this section.

**Left out on purpose:**

- An `Image` class. `Swapchain`'s depth image and `Texture` repeat about 30 lines of setup, and
  that class would remove the repeat and the smell in `Swapchain`. It is not needed for a
  textured cube.
- Bindless textures, where every texture sits in one array and the shader picks one by index.
  Nothing measured asks for it.

**How it was checked**, 2026-10-15, on the RTX 4060 Laptop GPU:

| test | result |
|---|---|
| validation, Debug, run and close | no `[WARNING]`, no `[ERROR]`. The leak report shows 0 blocks |
| UVs shown as colour | black, red, green and yellow corners on every face, as expected |
| textured cubes | skull and Mario on alternating cubes. Checked by eye, by Vakarian |
| missing texture file | `failed to load texture assets/textures/missing.png: can't fopen`, exit code 1 |
| validation, Release | no messages in nine 20-second runs |

**What it measured.** Release, `Record` average in ms. Three builds, interleaved: one 5-second
warm-up each, then three 20-second runs each, in the order milestone 6, one texture, two
textures. About 1,188 frames per run, 1,000 draws per frame.

| build | run 1 | run 2 | run 3 | median | min |
|---|---|---|---|---|---|
| milestone 6, no texture (`HEAD`) | 0.120 | 0.119 | 0.119 | **0.119** | 0.045 |
| milestone 7, one texture: 1 set bind per frame | 0.119 | 0.119 | 0.114 | **0.119** | 0.045 |
| milestone 7, two alternating: 1,000 set binds per frame | 0.160 | 0.160 | 0.150 | **0.160** | 0.061 |

`MakeDrawList` stayed at 0.017–0.019 ms in all nine runs. The frame arena's high water mark is
72,000 bytes, 1,000 items of 72 bytes.

What it says:

- **Textures cost nothing when draws share one.** One texture matches milestone 6 to the
  microsecond. The extra 4 bytes per `DrawItem` and the texture check per item do not show.
- **A set bind costs about 41 ns of CPU recording.** Two textures add 0.041 ms for 1,000 extra
  binds. That is 34% more `Record` time, well above the 8% run-to-run noise, and the minimums
  moved the same way: 0.045 to 0.061 ms.
- **At this scale it does not matter.** 0.16 ms is 1% of a 16.7 ms frame.
- **At milestone 6's million draws it would.** This is an estimate, not a measurement: a bind per
  draw would add about 41 ms to the 37 ms already measured. Sorting the list by texture, then
  mesh, cuts that to one bind per texture. The sort waits until a scene has many textures.
- **Only the CPU side was measured.** `Record` times recording, not the GPU's work. The GPU cost
  of switching sets was not measured.
- **Milestone 6's number held across sessions.** It read 0.111 ms on 2026-10-14 and 0.119 ms
  here, inside the noise.

**Cleanups after milestone 7.** Done 2026-10-15: the unused includes, the stale comments in
`FrameLoop.cpp` and on `DrawItem`, and the member order in `ResourceStore.h`. Left as they are,
by choice:

- `kMaxTextures` sits at namespace scope in `TextureDescriptors.h`, not inside the class.
- Rider reformatted all of `CMakeLists.txt`, so its diff is 173 lines for two new blocks.

### Descriptors — the terms, as settled in milestone 7

Written 2026-10-15, after the milestone 7 recap. The word "set" means two things, so the terms
are pinned down here.

| term | in the shader | in Vulkan |
|---|---|---|
| descriptor | — | one pointer: to a buffer, or to an image view and a sampler |
| binding | `binding = N` | a descriptor's position inside a set. An array binding holds several descriptors |
| set | `set = N` | a group of bindings. Its shape is the **layout**; a filled copy is a **set object** (`VkDescriptorSet`) |
| pool | — | the memory set objects come from. Each set object takes its own section |

What the recap settled:

- **A descriptor points at data; it never holds it.** The shader follows the chain at draw time:
  set object → descriptor → image view → `VkImage` → `VkDeviceMemory`.
- **Commands run after recording.** Every `vkCmd*` call records; the GPU runs it after the submit.
  Whatever a recorded command points at must hold still until the GPU is done. A push constant is
  the exception: the command carries the value itself.
- **So anything that differs between draws needs its own copy:** a set object per texture, or a
  push constant. The camera gets away with one buffer per frame slot because every draw in the
  frame reads the same value.
- **Set number = how often the binding changes.** Set 0 is what every draw in a frame reads,
  whether or not it changed since last frame. Higher sets change more often.
- **The set order only pays off with a second pipeline layout.** With one layout, binding any set
  leaves the others bound. When the layout changes, sets below the first differing one survive.
  Slow-changing data goes low so it survives pipeline switches.
- **The tools for per-draw data:** a push constant up to 128 bytes, then a dynamic uniform buffer
  (one set object, a new offset per bind), then an array indexed by the shader (needed for
  instancing and bindless textures).
- **A new pipeline is for a different program, vertex format or fixed-function state:** blending,
  depth, culling, topology, attachment formats. Different numbers go through descriptors or push
  constants. Check Vulkan 1.3's dynamic state before adding a pipeline.
- **Optional textures get a default:** a 1×1 white, black or flat-normal texture, always bound, so
  the shader never branches.
- **Limits to design for:** 4 bound sets guaranteed, 128 bytes of push constant guaranteed, 16 KB
  per uniform buffer guaranteed.

### Milestone 8 — the plan, and what it measured

Planned 2026-10-16. Done 2026-10-20, built as planned, with the light array brought forward.

**Objective.** Mario loads from an OBJ file and stands lit by Phong shading, with a texture, a
diffuse falloff and a specular highlight. A second material on a cube looks different under the
same light, which proves the highlight comes from the material and not from the shader.

**Decisions taken, and why:**

| decision | reason |
|---|---|
| **The front face is counter-clockwise, and the cube was rewound** | OBJ, glTF and Blender all wind counter-clockwise. One rule for every file means no loader swaps indices. Each cube triangle had its 2nd and 3rd index swapped |
| **The normal replaces the colour in `Vertex`, which stays at 32 bytes** | No shader read the colour after milestone 7. Position, normal and UV are 8 floats |
| **tinyobjloader, one header in `external/`** | It triangulates, handles negative indices and reports missing attributes. Real engines load glTF, so OBJ is a learning format and not worth a parser of our own. Taken from the `release` branch at commit 62ff207 |
| **`TINYOBJLOADER_DISABLE_FAST_FLOAT`** | The fast float parser bundled at that commit fails under MSVC with C++20 (`error C3615`). The library's own parser is enough for a 117 KB file |
| **`LoadObjFile` is a free function, Vulkan-side, with no Vulkan calls** | It keeps no state between calls. `Vertex` carries Vulkan layout code, so the loader lives next to it. As a free function it runs with no GPU, so a test can load a file without a window |
| **The loader merges identical corners with exact float equality** | Every float comes straight from the file's text, so equal text gives equal bits. A tolerance would break the hash map: nearly equal floats can land in different buckets. The worst case of exact equality is a missed merge, never a wrong one |
| **`ResourceStore::CreateMesh(path)` returns a `MeshHandle`** | The same shape as `CreateTexture`. `Main` makes the call. The core-side loader stays deferred until a scene loads its own files |
| **Lighting runs in world space** | A light position in world units is easier to check than one in view space. `CameraUniform` gains the camera's position, which `VulkanRenderer` reads from the last column of the inverse view matrix. When `Camera` gets a position of its own, the renderer reads that instead |
| **Up to 4 lights, a count, and one ambient colour, from the start** | More lights is the first change after this milestone. An array now cost a few lines; adding it later meant editing the struct, the shader and the boundary a second time |
| **The lights live in set 0, binding 1** | They change at the camera's cadence: once per frame, the same for every draw. `kUniformBindings` gained one row. The camera row now includes the fragment stage, which reads the camera's position |
| **`LightUniform` is 160 bytes** | Every array element starts on a 16-byte boundary, so positions and colours are `vec4`. The data ends at byte 148, and a uniform block's size rounds up to a multiple of 16. Three `uint32_t` of padding fill the gap, and a `static_assert` pins the size |
| **The scene returns a `SceneLighting`: an ambient colour and a span of `Light`** | The span points into the scene's own storage and copies nothing, so lights can later live in the ECS. The renderer owns the limit: more than `kMaxLights` throws, naming the count |
| **Lights are plain scene data, not entities, for now** | Fixed lights keep the shader tests simple. The span lets a later system gather `Position` and a `PointLight` component into the same array |
| **The normal matrix is computed on the CPU, once per object, and pushed** | `FrameLoop` builds `transpose(inverse(mat3(model)))` per draw item, instead of the shader building it per vertex. It goes in the push constant as three padded columns, so the block is 112 of the guaranteed 128 bytes. Scenes still send only the model matrix. It moves next to the model matrix in a storage buffer when instancing arrives |
| **The highlight takes the light's colour, not the texture's** | Plastic, paint and skin reflect the light's colour. Metals tint it, which would be one more material value. Specular applies only where the diffuse term is above 0 |
| **`TextureDescriptors` became `MaterialDescriptors`: a texture at binding 0, a uniform buffer at binding 1** | Shininess and specular strength differ per surface, so set 1 now describes a whole material. `Texture` stays "just an image", so a material can point at several textures later |
| **One pool, with two size entries** | A pool budgets descriptors per type. Each material set takes one combined image sampler and one uniform buffer, so the pool needs room for 64 of each. `maxSets` is a separate cap on set objects |
| **Each `Material` in the store owns its uniform buffer and its set** | A material's buffer must live exactly as long as the material, and the count grows while the program runs. `MaterialDescriptors` owns only what lives for the whole program: the layout and the pool. That is the lifetime rule from the top of this section |
| **The material buffer goes into GPU-only memory through a staging buffer** | It is written once at load time and read every frame, like a mesh. There is no copy per frame slot, because nothing writes it after load |
| **`MaterialDesc` lives in `core/Material.h`** | `Main` fills it, so it must hold no Vulkan type. It is not a scene concept: a scene only names a material by handle |
| **`Scene1` is the milestone 8 scene, and `Scene0` stays** | `Scene0`'s 1,000 cubes with alternating sets are the setup every `Record` number since milestone 6 comes from, so it remains the benchmark |
| **Entities carry `Renderable`, `Rotation` and `Spin`** | `Renderable` is a mesh handle and a material handle, 8 bytes, always read together. `Spin` and `Rotation` are split because different systems read them: the spin system reads `Spin` and writes `Rotation`, and `MakeDrawList` reads `Rotation` only. The cubes have no `Spin`, so the spin query skips them without an `if` |
| **`MakeDrawList` is the extract step** | It reads `Position`, `Rotation` and `Renderable` and writes draw items. Components hold handles, never Vulkan types, so the ECS never learns which backend draws it |

**What we took from the old code:** the `LoadObj` call, the loop over `shape.mesh.indices`, the V
flip, merging duplicate corners through a map, and the Phong formulas: `max(dot(N, L), 0)` and
`reflect(-L, N)`.

**What we changed:**

- The old shaders compare a view-space vertex with a world-space normal and a world-space light.
  Ours keep everything in world space.
- `phong.frag` adds the texture to the specular term (`spec * ks + kt`), and `phong.vert` reads a
  field that does not exist, so it cannot compile.
- `multiPhong` adds the ambient once per light, so the scene brightens with every light.
- Shininess is hardcoded to 14. Ours comes from the material, with a specular strength.
- The old specular term is tinted by the texture. Ours uses the light's colour.
- The old light directions are computed per vertex and blended. Ours are computed per pixel.
- A file with no normals makes the old loader read `attrib.normals[-3]`. Ours throws with the
  file name, and prints tinyobjloader's warnings instead of dropping them.
- The old hash adds code to `namespace std`. Ours is a functor passed to the map.

**How it was checked**, 2026-10-16 to 2026-10-20, on the RTX 4060 Laptop GPU:

| test | result |
|---|---|
| front face switched before the cube was rewound | the cubes turned inside out, which proved the switch took effect. After the rewind they looked as before |
| normals shown as colour | one flat colour per cube face, smooth gradients on Mario |
| Mario loaded | 952 vertices and 4,059 indices, the highest index 951 |
| missing OBJ file | `failed to load mesh missing.obj: Cannot open file [missing.obj]`, exit code 1 |
| one white light | lit side bright, far side ambient only, white highlights. Checked by eye, by Vakarian |
| red light left, blue light right | each side took its own colour |
| five lights | throws `the scene has 5 lights, and the renderer takes at most 4` |
| two materials under one light | shiny and matte surfaces showed different highlights. Checked by eye, by Vakarian |
| Mario squashed to half height | `mat3(model)` shaded his top too dark; the inverse transpose shaded it brighter, matching the flatter shape. Checked by eye, by Vakarian |
| lighting schemes | keys 1 to 5 switched Studio, Sunset, Neon, RGB mixing and Campfire |
| validation, Debug | no `[WARNING]`, no `[ERROR]` on build, run and close. The leak report shows 0 blocks |
| validation, Release | no messages in six 20-second runs |
| test suites | MemoryMonitorTests, EcsTests and ArenaTests passed in x64 and Win32, Debug and Release |

**What it measured.** Release, `Record` average in ms, with `Scene0`: 1,000 cubes, two alternating
sets, so 1,000 set binds per frame. Two builds side by side in one session: one 5-second warm-up
each, then three 20-second runs each, interleaved. About 1,184 frames per run.

| build | run 1 | run 2 | run 3 | median | min |
|---|---|---|---|---|---|
| milestone 7, two textures (`c323c56`) | 0.140 | 0.140 | 0.143 | **0.140** | 0.066 |
| milestone 8, two materials | 0.131 | 0.143 | 0.118 | **0.131** | 0.061 |

`MakeDrawList` stayed at 0.013 to 0.017 ms in all six runs.

What it says:

- **No measurable change.** Milestone 8's median is 6% lower, but its own runs spread from 0.118
  to 0.143 ms, more than the gap between the builds.
- **That matches the expectation.** The bind is the same call. A set with two descriptors binds
  at the same CPU cost as a set with one.
- **Milestone 7 read 0.160 ms on 2026-10-15 and 0.140 ms here.** That gap is between sessions,
  which is why both builds ran side by side.
- **Only CPU recording was measured.** The Phong shader's cost is GPU time, and `Record` does not
  time it.

**Left out on purpose:**

- Falloff with distance. A light's position only sets its direction, and a far light shines as
  bright as a near one.
- Tone mapping. Light colours add and clip at 1, so each lighting scheme keeps its total near 1.
- Lights as entities, Blinn-Phong, normal maps and `.mtl` parsing.
- A metal option, where the highlight takes the surface's colour.

**Still in the code after milestone 8:**

- The temporary `std::cout` of mesh counts in `ResourceStore::CreateMesh`.
- `MaterialDescriptors.h` includes `Buffer.h` where a forward declaration would do.
- `phong.vert` starts with four spaces before `#version`. glslc accepts it.
- The profiler label in `Main.cpp` still reads `Scene0::Update`, though `Main` builds `Scene1`.

**Next, after the Vulkan milestones:** the scene switcher, debug lines, the system registry and
physics, planned in the plan that ended on 2026-11-03 (dev-notes/README.md, "Plan steps"). More lights wait until after that: lights become entities with a
`PointLight` component, gathered into the span each frame, and the shader gains falloff.

### What is in `2026_GPUProgramming`

```
VulkanRenderer.cpp        919 lines    instance, device, swapchain, pipeline, sync
VulkanRenderer.h          341
DescriptorSetBuilder.cpp  217
VulkanPipeline.cpp        218
VulkanSampler.cpp         126
VulkanMesh.cpp            123
CoreStructs.h              92
VulkanCommandBuffer.cpp    71
```

About 2,200 lines, plus shaders with compiled `.spv` beside them, plus an OpenGL backend — so
the two-backend idea in "Two render backends" is already prototyped there.

**The Vulkan code is worth reading. The `Scene` and `Renderer` interfaces are not worth
keeping.** Two reasons, both structural:

- `Renderer::CreateWindow(name, width, height)` — the renderer creates the window. Backwards
  here, because `core/Window` already owns SDL and was built with the Vulkan flag set so a
  surface could be asked for later.
- `Renderer::Render()` takes no arguments, so the renderer must already own what it draws.
  That is the opposite of the draw list in "Two render backends".

`Scene.h` there is 18 lines, its constructor takes a `Renderer*` and discards it, and its only
member is commented out. It is a course template, and the name collides with the Scene this
engine needs.

### What the Ubisoft API is

About 2,100 lines including vendored freeglut, stb_image and miniaudio.

- **It owns `main`.** `app.h` forces `wWinMain` in, and `main.cpp` declares the game as four
  externs: `Init()`, `Update(float)`, `Render()`, `Shutdown()`. It calls you.
- **It is immediate mode.** `App::DrawTriangle(p1x, p1y, p1z, p1w, ..., r1, g1, b1, ...)` and
  `App::DrawLine(...)`, every frame, with loose floats. No handles, no buffers, no meshes.
- **It is 2D, freeglut, OpenGL 1.x shaped.** Virtual 1024x768 coordinates, GLUT bitmap fonts,
  `glFlush()`.
- **It bundles input and audio**, not only graphics: `App::PlayAudio`, `App::IsKeyPressed`,
  `App::GetController`.
- **It has its own loop, profiler and 60 fps cap.**

The two backends are about as different as two backends get. That is useful: it makes the
boundary obvious rather than a matter of taste. The only thing both can consume is a draw list.

### The open questions

**1. ~~Does `Render()` draw, or produce a draw list?~~ Answered: draw list.** `Scene` has no
`Render()`. It has `virtual void MakeDrawList(DrawList& drawList) = 0`, so the scene fills an
array the caller owns and the backend consumes it. The second backend stays possible.

Milestone 6 decided the shape of an entry: a `DrawItem` is a `MeshHandle` and a model matrix,
68 bytes. Milestone 7 added a texture handle, which made it 72. Milestone 8 renamed it to a
material handle, at the same size.

Two consequences worth knowing. The caller owning the storage is what lets a frame arena stand
behind it — see dev-notes/memory-manager.md. And the size adds up: 6.8 MB per frame at 100,000 entities, and a
measured 68 MB per frame at a million (see "What milestone 6 measured"). That runs straight into
the open question about where render-only data lives (now ROADMAP.md, Rendering, "Frustum culling and transforms", row 1).

It also settles the retained-versus-immediate question left open since the start of the design:
the *boundary* would be immediate — one fresh list per frame — with the Vulkan backend caching
buffers and pipelines behind it, and the appendix's qualification of rule 7 applying there and
nowhere else.

**2. ~~What shape is `Scene`?~~ Answered: driven from outside.** `include/core/Scene.h` has
`OnCreate`, `OnDestroy`, `Update`, `MakeDrawList`, `MakeDebugLines` and `HandleEvents`, and
`Application` calls them through `SceneManager` (step 1, built 2026-10-21: dev-notes/scene-switcher.md). Each scene
owns its `World`. The reasoning, as written before the answer:

The lifecycle from the Vulkan course project — `OnCreate`,
`OnDestroy`, `Update(dt)`, `Render()`, `HandleEvents(event)` — is familiar and costs no new
mental model. What changes is the inside: it owns a `World` and a `CommandBuffer`, where the
old one owned nothing.

The constraint worth respecting whatever is chosen: **Scene must be driven from outside.** Own
the loop and call it, or let Ubisoft's `Update()`/`Render()` callbacks call it. A Scene that
reaches out and grabs a swapchain itself only ever works with Vulkan.

**3. ~~Which backend first?~~ Answered: Vulkan.** It is the coursework and the priority, and it
is the one being built. Whether the Ubisoft backend ever gets built is still open. The seam is
in place either way: the scene fills a `DrawList` and never sees a Vulkan type.

**4. ~~What is the first visible milestone?~~ Answered: all three, in order.** A cleared screen
was milestone 2, one triangle was milestone 3, and one cube per entity read from `Position` was
milestone 6, done 2026-10-14. That was the first time the ECS and the renderer met, and it
decided the shape of a `DrawItem`.

**It also made `Main.cpp` a real program.** In September `Main.cpp` was a test suite that
happened to be the entry point. The checks moved to `tests/` on 2026-09-29, and `Main.cpp` is
now about 15 lines: `Application` owns the window, the renderer and the loop (step 1: dev-notes/scene-switcher.md).
The real components that "what the game actually is" asked for arrived with the scenes:
`Renderable`, `Rotation` and `Spin` in Scene1, and many more in the prototype games
(findings/2026-10-prototype-game.md).

**5. ~~The Ubisoft contest: reuse this engine, or rebuild it?~~ Moved to living/ROADMAP.md on
2026-10-23,** where open questions about future work live.

### What is not at risk

**Memory management.** The arena (dev-notes/memory-manager.md) sits under the ECS — archetype columns, the
command buffer's payload, frame scratch. Vulkan's device memory is a separate allocator and the
two never touch. Nothing done to the arena is coupled to a backend.

**The ECS itself.** Rendering plugs in as a system. No component, archetype, query or command
buffer change is implied by any answer above.

---

