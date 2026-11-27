# Roadmap

**Last updated:** 2026-11-26.

## Contents

- [ECS](#ecs)
  - [Hardening](#hardening)
  - [Worth weighing](#worth-weighing)
  - [Concurrency](#concurrency)
- [Physics](#physics)
  - [Collision](#collision)
  - [Rigid bodies](#rigid-bodies)
  - [Fixed step additions](#fixed-step-additions)
- [Math](#math)
  - [Owning the math library](#owning-the-math-library)
- [Rendering](#rendering)
  - [Frustum culling and transforms](#frustum-culling-and-transforms)
  - [Basic shapes](#basic-shapes)
  - [Model import settings](#model-import-settings)
  - [After ray query shadows](#after-ray-query-shadows)
  - [After the skybox](#after-the-skybox)
  - [ImGui debug UI](#imgui-debug-ui)
  - [Game UI](#game-ui)
- [Debug tools](#debug-tools)
  - [Persistent debug lines](#persistent-debug-lines)
  - [A shipping build](#a-shipping-build)
  - [Automated game tests](#automated-game-tests)
- [Networking](#networking)
  - [After the Boss Fight Scene](#after-the-boss-fight-scene)
- [Input](#input)
- [Audio](#audio)
- [Platform](#platform)
- [Miscellaneous](#miscellaneous)
- [Code cleanup](#code-cleanup)

## ECS

### Hardening

Step 3 measured `Playback` on 2026-10-26: about 0.6 microseconds per frame in Scene0, down from about
0.7, so it did not grow and this block did not need to come first. None of it blocks other work: the
physics scenes hold a few hundred entities, created once.

| # | builds | why |
|---|---|---|
| 3 | Handles inside component data resolved at playback, or a create that returns the real handle | The command buffer resolves placeholders only in a command's target, never inside a component's bytes. So today an entity that holds another entity's handle must be built straight into the World in `OnCreate`. Create with components is built (dev-notes/create-with-components.md), so a create already carries every value in one command |
| 6 | Run conditions: `Add` takes an optional function that reads world data and answers run or skip. `RunPhase` asks it before each run. The C and M toggles become a `DebugView { showWires, showMeshes }` in the World's `Resources` (dev-notes/resources.md), written by an ordinary system in Update | Step 4a switches systems with a flag on `System` that only `SystemRegistry::SetEnabled` flips. That works for two toggles, but the on/off state hides in the registry. As data in the world, it can be printed, saved and shown like any component, and one condition (a `GamePaused`, say) can gate many systems at once. Bevy's `run_if` and Unity's `RequireForUpdate` work this way. The flag's `continue` in `RunPhase` stays; only the question changes. The store's mask can answer "are all of this system's resources there" with one AND |
| 8 | Stable type identity: identify a type by a hash of its compile-time name, keep the dense id for runtime, and register in the exe only | `IdOf<T>` is a header static, so each DLL gets its own counter, and the same mask means different things across the boundary. Needed by the first DLL, save or network packet |
| 9 | Faster migrations: a column lookup table per archetype (`FindColumn` becomes an array read), walking only the shared bits of the two masks in `MoveEntity`, and an archetype edge cache (the archetype reached by adding component X) | Only when the counters show migration cost. Measured 2026-09-30 with `EcsBench`: 0.114 to 0.124 microseconds per migration from 1,000 to 100,000 entities, and 24,000 migrations fit in a 4.2 ms frame. Missing 60 fps would take about 130,000 migrations in one frame (dev-notes/profiling.md) |
| 10 | Enableable components: one bit per entity per component, toggled with no structural change, honoured by queries | The answer if archetype churn ever dominates. Adding a tag is a full row copy here, against a push into one pool in EnTT. The counters say when |
| 11 | Writes to other entities sorted by target first | A damage system that reads a contact and writes `Health` on another entity chases a pointer per write. Sorting the events by target is the usual answer. Only once it shows in a profile |
| 13 | A chunk size per archetype, overriding the World's `chunkBytes` | Only once the 4, 16 and 64 KB benchmark (dev-notes/ecs-chunks.md) shows that different archetypes want different sizes: a wide row fits few entities in a small chunk, and a tiny row wastes a large one. Unreal Mass has this as `ChunkMemorySize` in `FMassArchetypeCreationParams`, where 0 means the default |
| 14 | `Query::single()`: runs the query and checks for exactly one match | For one-of-a-kind entities that share behaviour with others, such as the player. Shorter than a loop that runs once, and it fails at once if a spawn bug makes zero or two |
| 15 | Declared resource access: a `System` lists the resources it reads and writes, as it lists components, and `SystemContext` gains a read-only `Get` | Today any system can call `Get<T>()` and change any resource, the input and the camera included. On one thread that is harmless. Once systems run in parallel, the scheduler must know who writes what, or two threads write one resource at once. Bevy splits `Res` from `ResMut` for this. Comes with the threading work |

**Row 9, a further option: power-of-two rows per chunk.** Round `rowsPerChunk` down with
`std::bit_floor`, and the row-to-chunk lookup becomes `row >> rowShift` and `row & rowMask` instead of a
divide. The cost is chunk space: a row that fits 300 times gets 256, and one that fits 511 times wastes
about half the chunk. The lookup runs only in `GetComponent` and migrations, never in the chunk loops,
so it comes with row 9's trigger: the counters show migration cost.

**Row 10, the alternative: sparse-set storage.** Bevy lets a component opt out of the archetype tables
and live in a sparse set. Adding and removing it then moves nothing, but iterating it gets slower. Bevy
suggests it for per-frame markers and short status effects; here that means `PickedComponent`, `FallingTag` and
`DebugWatch`. Same trigger as row 10: the counters show archetype churn. Weigh both when it fires.

**Row 8 and networking.** A packet needs stable type ids only if it carries component ids. The
networking scene sends snapshots in a fixed, hand-written layout (network ID, position,
orientation), so row 8 can wait for saves or DLLs.

Rows 1, 2, 4, 7 and 12 were built by the ECS hardening plan that ended on 2026-11-05, and row 5 by
the resources plan that ended on 2026-11-19 (dev-notes/README.md, "Plan steps"); the numbers of the rows left keep their
old values, so references to them still hold.

### Worth weighing

Not planned, and maybe never needed. Each row records a trade and the sign that would make it worth
building. Until that sign shows, the row only reminds.

| # | builds | why |
|---|---|---|
| 1 | Lifecycle hooks: a ctor, dtor and move function per component type, stored by `Register<T>()` and called by migrations, swap-remove, `Clear`, chunk returns and new rows. Trivially copyable types keep the `memcpy` path | Rule 4 keeps every component plain bytes, so no component can own a socket, a vector or a map. Flecs lifts that limit with hooks, and Bevy 0.19 then stores resources as components on entities. The command buffer is the expensive part: it stores components as raw bytes and must move values in and out instead. The risk is what hooks allow: a vector in a hot component keeps its elements off the chunk, and iteration loses its cache benefit. Rule 4 forbids that today; with hooks, only discipline does. The sign: an observer, a hook, an entity inspector or saving starts needing its own version for resources, which live in a separate store, `Resources` (dev-notes/resources.md) |

### Concurrency

| # | builds | why |
|---|---|---|
| 1 | Systems split across threads | Effort medium (a guess from 2026-09-27). Barely changes the ECS: one command buffer per thread, merged at the sync point |

Systems and phases' design lists what threads need beyond the command buffers: a reader-and-writer
check, `Conflicts(a, b)`, next to `Print`'s shared-writes note; one command buffer per thread, merged
in a fixed order; a thread pool; and declared reads and writes for shared data that is not a
component, such as the `DrawList`. A render thread would also need one frame arena per frame in
flight. A guess, not measured: if the three habits in ECS.md's "Systems and phases" hold, this is
one class of a few hundred lines plus one line per system, and no system body changes.

## Physics

### Collision

Shapes in `PhysicsScene` pass through each other and through the floor, and nothing stops a released
shape from falling forever: F3 restarts the scene instead. Step 4 left collision out of scope.

| # | builds | why |
|---|---|---|
| 1 | Shape-against-shape collision: detect contacts between the four colliders, then resolve them | The colliders and their ray tests exist since 4b, and the constraint solver since 4c, so a contact can become one more constraint in Fixed |
| 2 | A floor | The cheapest first contact, and it ends the endless fall |

### Rigid bodies

From the non-uniform scale bonus, 2026-11-14 (dev-notes/non-uniform-scale.md).

| # | builds | why |
|---|---|---|
| 1 | Build the mass properties from the `TransformComponent`, so the scale is written once | Each body states its scale twice, in the `TransformComponent` and in the `InertiaTensor` call, and nothing keeps them equal. Stretching a sphere's `TransformComponent` alone made it spin like the old round sphere, and the uniform-scale assert never saw the stretch. A tensor that differs from the shape on purpose, for feel, would be set by hand afterwards |

### Fixed step additions

From step 3's design of `FixedStep`. Each comes when something needs it.

| # | builds | why |
|---|---|---|
| 1 | `Alpha()`: the leftover time divided by the step, for interpolation | A frame can run 0 or 2 fixed steps, which can stutter. Interpolating draws between the last two steps hides it. The Pacing Scene's bottom row shows the problem: at 240 Hz it holds each position for 4 refreshes, and the eye sees it blur (dev-notes/live-fps-and-pacing-scene.md, step 7) |
| 2 | Pause and single step: freeze physics and advance one step per key press | For debugging physics frame by frame |
| 3 | A step counter | For networking, replays, or comparing two runs |
| 4 | A time scale, for slow motion | If it must also slow Update, pause and time scale may need a third class between `Timer` and `FixedStep`, as Bevy's `Time<Virtual>` |
| 5 | The step size as a setting | If constraints need 120 Hz |

Glenn Fiedler's "Fix Your Timestep!" is the standard write-up.

## Math

### Owning the math library

Decided 2026-10-28: Humber's math library is ours now. It has been edited many times (`kPi`, the
trivially copyable types, `kVerySmall`), and 4b deleted parts of it.

| # | builds | why |
|---|---|---|
| 1 | Move it out of `external/` into the engine's own folders | The folder still says "someone else's code". A namespace should match its folder |
| 2 | Restructure it, and drop what nothing uses | The `MATHEX` dual quaternion and 2D sets may be unused. Check before deleting |
| 3 | Move general math into `MATH`: `PHYSICS::SolveQuadratic`, and maybe `PHYSICS::Ray` | They are not physics. Until the move they stay in `PHYSICS`, so each name matches its folder |
| 4 | Add license files for `external/MathLibrary` and `external/glm` | Neither came with one. Needed before the repository goes public |
| 5 | Turn `VERY_SMALL` and `VERY_CLOSE_TO_ONE` into constants | They are still macros in `ConstantsConversions.h`, where `kPi` already became a constant |

## Rendering

### Frustum culling and transforms

Rows 1 to 3 need culling (row 4), or the ECS hardening above, before they pay off. The camera already
reaches systems: it is a resource, read with `context.Get<Camera>()`.

| # | builds | why |
|---|---|---|
| 1 | `StaticMatrix`, a matrix component built once at load. Holding it means the entity never moves, so the TRS build skips it with the query's `none` mask and the draw system reads it straight | Culling is its second reader. To weigh: a 64-byte matrix in the same archetype as gameplay data halves the rows per block, which raises the copy cost of every structural change. Unity stores `LocalToWorld` and Bevy `GlobalTransform`, both derived, both written by one system, both documented as stale for part of the frame. Cache only what never moves: a mover pays the build either way, and 4,001 of them cost 0.124 ms |
| 2 | Parent and child transforms | Scene3's turrets copy their parent's transform by hand. Parents must update before children, and archetypes group by which components an entity has, not by a value such as depth (house rule 10). The candidates: sort blocks by depth, make depth part of archetype identity as DOTS does, or rebuild a per-depth index. Needs 1 |
| 3 | Shared fragments, and one instanced draw per archetype | Scene3 drew 2,944 items one draw at a time. A shared value splits archetypes, so it needs ECS hardening step 1 |
| 4 | Frustum culling | Effort medium (a guess from 2026-09-27). Does not change the ECS: it is a system |

Rejected 2026-10-26 for row 1, after reading how Unity, Bevy and Legion do this. **A per-frame dirty
tag:** adding and removing a component is a structural change at about 8 migrations per entity, so
marking 4,000 movers would cost thousands of migrations a frame to save 0.124 ms. No shipping ECS
marks dirtiness by adding a component. **A write version per column:** nearly free to track, but an
archetype holds every row in one run, so one drifting rock dirties all 4,000 beside it. Worth
revisiting once ECS hardening step 1 splits the columns into blocks.

A camera **entity** is not planned. The trigger for it is the engine having to loop over cameras:
split screen, a shadow or minimap view, a camera rendering to a texture. One view is a field; several
views are a column. Unity keeps the camera out of the ECS for this reason, and Bevy puts `Camera`,
`Frustum` and `VisibleEntities` on one entity so that N views are N entities. Follow cameras, shake
and splines do not need it, and `FlyCameraController` already shows that.

### Basic shapes

From physics step 4a's design, 2026-10-27. Generated meshes for the basic shapes, beside the existing
`CreateCube()`, each taking its size the way the matching collider does. Wanted for level blockouts,
placeholders, prototypes and test scenes; all three prototype games were built from cubes. Not needed
for step 4: the physics scene reuses Mario and the cube.

| # | builds | why |
|---|---|---|
| 1 | `CreateSphere()`, `CreateBox(halfExtents)`, `CreateCylinder(radius, halfHeight)`, `CreateCapsule(radius, halfHeight)` on `AssetLoader`, generated in code and cached per size | `TransformComponent.scale` is uniform, so it cannot change a shape's proportions: a 1 m cube mesh can never become a 2 x 1 x 4 box. A mesh generated at the collider's sizes matches it at any proportions, and scale still multiplies both. The sizes follow the collider convention, half sizes measured from the centre. A ball has one size, so one unit sphere and `scale` cover every ball. Godot builds its primitives this way (`CapsuleMesh` has `radius`, `height`, `radial_segments`, `rings`); Unity's `CreatePrimitive` uses fixed built-in meshes |

**Decided against: a spawn helper** that creates the mesh, the entity and the matching collider in one
call, as Unity's `CreatePrimitive` does. Creating a mesh and creating a collider are separate actions:
the mesh is a shared asset in `AssetStore`, and the collider is a component on one entity. Nothing
links them except the same numbers passed to both. Keeping the two calls visible in scene code keeps
that clear.

### Model import settings

From physics step 4a's design, 2026-10-27. A collider is centred on the entity's origin and sized by
`TransformComponent.scale`, so a model in the wrong units or with its origin off centre puts the collider in
the wrong place, or makes every collider number read in odd units. Not needed while the physics scenes
use centred unit meshes.

| # | builds | why |
|---|---|---|
| 1 | Import settings passed with the load, such as `CreateMesh("Mario.obj", {.fitSize = 1.0f, .recentre = true})`, and part of `AssetStore`'s cache key | The store caches meshes by path and shares them between scenes, so a separate `AdjustMesh` call after the load would change the mesh for every scene and miss the copy already on the GPU. With the settings in the key, the same file with and without them becomes two meshes |
| 2 | An import scale: a known unit factor per file (centimetres to metres is 0.01), or fit to a size from the bounding box | Keeps `TransformComponent.scale = (1, 1, 1)` meaning "the size the model is meant to be", so collider numbers are plain metres and scale stays free for gameplay. A uniform scale cannot deform the mesh: positions are multiplied by one number, and normals, UVs and indices do not change |
| 3 | Recentre: subtract the bounding box centre from every vertex | The engine can measure where the origin sits but cannot know whether it was meant: a character's origin at the feet lets it stand at y = 0, and a door's sits on its hinge. So recentring is a choice made per model, never automatic. Where the origin is meant, the alternative is an offset on the collider, which Unity's colliders have as `center` and step 4a left out |
| 4 | The facing fix: turn a model that faces +Z to the engine's -Z at load | Space prototype finding 8. `Mario.obj` faces +Z, and the prototype paid a quaternion multiply per draw for it |

### After ray query shadows

From the ray query shadow design talk, 2026-11-11, and what its plan found (ended 2026-11-14,
dev-notes/ray-query-shadows.md). The plan kept a seam open for rows 1 to 3: `shadowFactor` in the
shader, the TLAS split into fill and build, and one source of transforms in the `DrawList`. "Step N"
means a step of that plan.

| # | builds | why |
|---|---|---|
| 1 | Shadow maps as a second body for `shadowFactor`, picked by the shadows switch (Ctrl+K) | Ray query needs a GPU with ray tracing support, so players on older cards get no shadows without a fallback: about 16% or more of Steam players in October 2026. Shadow maps are not always faster: they pay a depth pass per light, ray query pays per pixel per light (0.26 ms in Scene1 with its floor), so the GPU timers decide |
| 2 | A GPU scene buffer: one storage buffer with every object's transform and material index, written only where an object changed, read by index in the shaders | Today each `DrawItem` costs one push constant and one draw, and the CPU sends every transform every frame. The buffer allows one draw for many objects and GPU culling, and lets raster, the TLAS and compute read one copy. Only once `Scene0` shows draw call cost. Rendering row 3 of "Frustum culling and transforms" (instanced draws) is a neighbour |
| 3 | A compute shader that fills the TLAS instances from the GPU scene buffer | Moves the per-frame instance loop off the CPU, so transforms never travel back through it. The TLAS build reads only the instance buffer, so it does not change. Needs row 2: without it, the CPU uploads the transforms anyway. No source confirmed this as common practice; Unreal has a flag for a GPU copy of instance transforms |
| 4 | Batched BLAS builds with compaction: every mesh's build in one command buffer with one shared scratch buffer, then a compacted copy of each BLAS | Step 4 builds one BLAS per mesh with its own submit, wait and scratch buffer. Batching waits once and shares the scratch, which NVIDIA's tutorial does mainly to stay inside a memory budget. Compaction copies each built BLAS into a smaller buffer; NVIDIA claims 20 to 50% savings without measurements, and it fits a batch: build, query the compacted sizes, copy. With a handful of cached meshes neither shows (a guess, not measured). Start when load time rises with mesh count, when streaming arrives, or when BLAS memory becomes a concern |
| 5 | A fix for the shadow terminator: the jagged self-shadow where a low-poly surface turns from the light | Seen on Mario's boots, cap brim and gloves. A bigger ray offset hides it but thins every shadow, which costs small objects most, so the offset stayed 0.01 (step 7). The targeted fixes: Ray Tracing Gems II, chapter 4 ("Hacking the Shadow Terminator", Hanika), and Disney's "Taming the Shadow Terminator"; neither was read in full |
| 6 | Skip the TLAS fill and build while shadows are off | Both run every frame whatever the switch says: 0.120 ms of GPU time in Scene0's 1,000 instances (step 7's table). Nothing else reads the TLAS yet, so off can skip it. Ambient occlusion or reflections would read it too |
| 7 | TLAS update mode instead of a full rebuild | Step 5, decision b chose a rebuild every frame with `PREFER_FAST_BUILD`, and named update mode as the next move once the build zone shows a real cost. It reads 0.120 ms for 1,000 instances. An update needs the same instance count as the last build, so scene changes still rebuild |
| 8 | Soft shadows: a light with a radius, several rays to random points on it, and a denoiser or frame accumulation | One ray to a point light gives only 0 or 1, so edges are hard. The Khronos tutorial calls soft shadows "native" to ray query because no new technique is needed, but the rays multiply and few rays are noisy. Shadow maps soften with PCF or PCSS instead |
| 9 | A switch that forces ray query and timestamps off on a GPU that has them | Every ray query and timestamp branch has a path for GPUs without them (no binding 2, no second pipeline, no GPU times), and no run has taken it: both GPUs on this machine support both. Since the skybox, set 0 without ray query is bindings 0, 1 and 3, a gap in the binding numbers that no run has built |

### After the skybox

From what the skybox plan found (ended 2026-11-17, dev-notes/skybox.md). "Step N" means a step of
that plan.

| # | builds | why |
|---|---|---|
| 1 | Mipmaps for `Texture` and `CubeMap`: a level chain built with `vkCmdBlitImage` after the upload, one chain per cube face, and `maxLod` raised to match | Both create `mipLevels = 1`, and their samplers set `maxLod = 0`. When a texel is smaller than a pixel, the GPU picks one texel and skips its neighbours, so fine detail shimmers as the camera turns. Vakarian found the skies low quality; the faces are 2048, about one texel per screen pixel, so missing mipmaps are the likeliest cause, not the size (a guess, not measured). Anisotropy is on but does little without levels |
| 2 | Compressed sky faces: BC7 for colour skies, or BC6H for HDR ones, converted when a sky is added | Each 2048 sky takes 6 × 16 MB = 96 MB as `R8G8B8A8_SRGB`, and Scene1 loads three (arithmetic, not a measurement). BC7 holds a texel in 1 byte, a quarter of that. BC6H also removes the banding of 8-bit gradients, such as the sunset haze |
| 3 | The sky facing check by script: `run-game.ps1` turns the camera along +X, -X, +Z, -Z, up and down over the marked test sky, and takes a screenshot each time | Step 6, decision b planned it, and it never ran: Vakarian checked the facing by eye. A swapped face or a sign change in `skybox.frag` would then show without anyone looking. `assets/skyboxes/test/` already holds the marked faces |
| 4 | Sky source sheets and the test sky out of `assets/` | CMake copies everything under `assets/` next to the executable on each build. `night/` and `red_skies/` each still hold their 6144 × 4096 source sheet, `sky1.png`, which nothing loads, and `test/` loads nowhere in the game |

### ImGui debug UI

| # | builds | why |
|---|---|---|
| 1 | Dear ImGui debug panels over the game, with its official `imgui_impl_vulkan` and `imgui_impl_sdl3` backends | Stats, toggles and tuning values on screen, without a key for each one. Comes before the game UI below, so the panels can tune it |
| 2 | Later: an own renderer in place of `imgui_impl_vulkan`, on the game UI's 2D pipeline | For the learning. No speed gain at this scale |

From the same prototype, 2026-11-14. It used Dear ImGui 1.92.0, drew a panel in Scene1, took a click
without moving the camera, and survived a resize. Estimate: row 1, 1 to 2 sessions; row 2, 4 to 6
sessions.

Outline considerations for row 1:

- ImGui ships as source files: its core and the two backends go in `external/imgui`, compiled into
  the game.
- The ImGui context lives in `app`, as a member of `Application` declared before the renderer, so it
  starts before the Vulkan backend and outlives it.
- Every ImGui Vulkan call stays inside `vulkan/` (architecture rule 1). `DrawFrame` takes an
  `ImDrawData*`, an ImGui type, not a Vulkan one. `GPUContext` needs a `GetInstance()`.
- The prototype drew the UI from a second command buffer per frame slot, in the same submit after the
  scene's, with a barrier in between and no semaphore.
- Input order: app keys first, then ImGui's events, then `InputState`, gated by `WantCaptureKeyboard`
  and `WantCaptureMouse`. One gate covers every system (architecture rule 8).

For row 2: the Khronos tutorial (Building a Simple Engine, GUI chapter) writes its own renderer but
leaves out its pipeline and shaders. ImGui's vertex (`ImDrawVert`: 2D position, UV, packed colour)
can be the game UI's vertex, so one 2D pipeline draws both.

**Things to watch out for**, from the prototype:

- **Key capture.** Once an ImGui window has focus, `WantCaptureKeyboard` stays on. The prototype
  checked it before the app keys, and F5 stopped hiding the UI.
- **Held state too.** Gating only the events is not enough: while ImGui wants the mouse or keyboard,
  `InputState`'s held keys and buttons must be cleared, or the camera moves during a slider drag.
- **Flush bug in 1.92.0.** The font texture upload flushes a range that is not a multiple of
  `nonCoherentAtomSize` (`imgui_impl_vulkan.cpp:786`): one validation `[ERROR]` per upload.
  `MinAllocationSize` does not cover it. A newer release may fix it (not checked); otherwise a
  one-line patch.
- **Fixed colour format.** The ImGui pipeline is built for the swapchain's format. A rebuild that
  changed the format would leave it stale; today the format never changes.
- **sRGB swapchain.** Expected washed-out colours did not show in the screenshots. Check again with
  custom style colours.
- **Not for player menus.** ImGui works as a stopgap HUD (images, hand-driven animation), but its
  keyboard and controller navigation is built for tools.

### Game UI

| # | builds | why |
|---|---|---|
| 1 | A HUD: textured, coloured quads on screen, anchored to a screen corner or the centre | Players see health icons and counters, and the HUD follows a resize |
| 2 | Sprite atlases: one texture holding many HUD pieces, picked by UV rectangle | One texture means fewer binds. `Texture` already loads PNGs, so no new image parsing |
| 3 | Text from a font atlas baked with `stb_truetype` | Counters and labels as text, one quad per letter, drawn by the HUD's pipeline |
| 4 | Menus: a state machine of screens and a selected button, fed by `InputState` | A controller and a keyboard work the same way |
| 5 | Animation: flipbooks (frames over time change the UV rectangle) and tweens (a value from A to B with an easing curve) | A health icon breaking is a flipbook; one popping in is a tween |

From the UI talk after the prototype, 2026-11-14. The UI players see, as in the Hollow Knight-like
prototype. Built in house, not with middleware. Estimate, a guess: 4 to 8 sessions for a usable HUD
and menus. Needs the ImGui block first, for tuning. Rows 2 to 5 each build on row 1.

Outline considerations:

- Row 1 needs a 2D pipeline with an orthographic projection, drawn in the UI command buffer before
  ImGui, so ImGui stays on top. Using `ImDrawVert`'s layout lets ImGui row 2 share it.
- The game fills a UI quad list each frame, as it fills the `DrawList`: texture, position, size, UV
  rectangle, colour. No Vulkan type in game code.
- Flipbooks and tweens only feed quad fields, so animation needs no renderer change.

Open for the design talk: whether UI elements become ECS entities with `UIQuad`, `Flipbook` and
`Tween` components and systems that advance them; where layouts live (code first, a data file later);
and RmlUi (HTML and CSS-like, MIT as recalled) as the fallback if menus grow large.

## Debug tools

### Persistent debug lines

| # | builds | why |
|---|---|---|
| 1 | A line added with a lifetime that `core/debug` keeps between frames | A system outside Extract could then draw once. Step 4b draws its pick ray each frame from a `PickRayComponent` instead. Unreal's `DrawDebugLine` takes a persistent flag and a lifetime (from memory, not checked) |

### A shipping build

From physics step 4a's design, 2026-10-27. Debug drawing (the `core/debug` line helpers and debug
systems such as `DrawColliders`) runs in every build today, turned on and off at runtime with keys, the
way PhysX ships its visualization switched off.

| # | builds | why |
|---|---|---|
| 1 | A define of its own for debug drawing, such as `GE5_DEBUG_DRAW`, wrapping the helpers and the lines that register debug systems, once the engine has a shipping configuration | Not `_DEBUG`: the wires are wanted in optimized builds too, for example while profiling a physics bug in Release. Unreal does the same, wrapping debug drawing in `UE_ENABLE_DEBUG_DRAWING` and removing it only from Shipping builds. An environment variable cannot do this job: it picks settings at runtime but cannot take code out of the binary |

### Automated game tests

From 2026-11-05. `scripts/run-game.ps1` drives the game from outside: it sends keys and mouse input
through Windows and photographs the window. Its first runs found a real crash (Scene1's world arena
fills at startup). It needs the desktop and the window's focus, its timing is wall-clock time, and its
screenshots depend on the screen. These rows move the same job inside the engine, so a run needs no
desktop and plays the same frames every time.

| # | builds | why |
|---|---|---|
| 1 | Input replay: a file of "frame N: this input" that fills `InputState` instead of SDL | Input enters in one place (STATE.md, architecture rule 8), so a replay changes nothing else. With the fixed step, a replay plays the same frames on every run, with no focus or desktop |
| 2 | Frame capture: the renderer saves a frame to a PNG on request | An exact image of what the GPU drew, with no screen, window border or DPI in it |
| 3 | Image compare: save a known-good image once, compare each run against it | Catches visual changes no log shows. Needs 1 and 2, so the same frames give the same image |
| 4 | `run-game.ps1` starts the exe with a replay file, and the PowerShell input goes | Once 1 and 2 exist, the script only starts, waits and reads the summary |

## Networking

### After the Boss Fight Scene

| # | builds | why |
|---|---|---|
| 1 | Prediction for the client's own movement, with reconciliation | The Boss Fight Scene moves the local player only after a round trip to the server. Prediction moves it at once, and reconciliation corrects it when the server disagrees |
| 2 | A listen server: a server World and a client World in one process | One player hosts with no second program. Per-World resources (STATE.md rule 12) already allow two Worlds |
| 3 | Client authority over its own movement | Needs no reconciliation, but trusts the client, which is fine for co-op |

## Input

| # | builds | why |
|---|---|---|
| 1 | Controller support | Effort small (a guess from 2026-09-27). Does not change the ECS |

`PlayerCommandComponent` already holds the game commands (move, aim), written once per frame by
`PlayerCommandSystem` from `InputState` (PLAN.md, E2.3). A controller writes the same component, so
no Fixed system changes. dev-notes/physics-scene.md has the input design it builds on.

## Audio

| # | builds | why |
|---|---|---|
| 1 | Audio | Effort small to medium (a guess from 2026-09-27). Does not change the ECS. The audio system is the one place allowed to keep "what changed since last frame" state, as house rule 7's exception for a retained API says |

## Platform

| # | builds | why |
|---|---|---|
| 1 | File I/O | Effort small to medium (a guess from 2026-09-27). Does not change the ECS |
| 2 | DLLs | Effort small (a guess from 2026-09-27). Does not change the ECS |

## Miscellaneous

Small changes that can land between any two steps.

| # | builds | why |
|---|---|---|
| 1 | A colour in `MaterialDesc` | A material's colour comes only from a texture today. Scene3 needed ten PNG files only for colours |
| 2 | A light budget in the renderer | The renderer throws past `kMaxLights` today |
| 3 | A way to pass settings to a scene | Scene3 read autoplay from an environment variable |
| 4 | Settle whether borderless fullscreen tears with Vulkan on this machine | In the Pacing Scene, IMMEDIATE never showed a clear tear, even at cap 100 and cap 30 in fullscreen (dev-notes/live-fps-and-pacing-scene.md, step 7). Windows may still compose the image. If so, the IMMEDIATE and FIFO_RELAXED settings show no tearing, and only exclusive fullscreen or a driver setting would |
| 5 | An action-mapping layer for the app keys | `Application::HandleEvent` writes every key into `InputState.pressed` before it checks for F1 to F4, G, Ctrl+P, Ctrl+L, Ctrl+K and Alt+Enter, so systems still see app keys. Ctrl+K (shadows) joined them (dev-notes/present-mode-and-fullscreen.md, step 4, decision 5) |

## Code cleanup

Known leftovers in the code, moved from STATE.md on 2026-11-03.

| # | builds | why |
|---|---|---|
| 1 | Automated tests for `PickSystem`, `DrawPickRaySystem` and the dangle systems | Nothing runs them automatically. Vakarian checked them by hand on 2026-10-30, 2026-11-02 and 2026-11-03; `PhysicsTests` covers only the math they call |
| 2 | A check that a system's query asks for the components it reads: an assert in `Column`, or a check in the registry | The read mask is only a label. In 4c, a query copied from another system matched archetypes without `PickedComponent`, so `Column<PickedComponent>()` returned null and crashed. It happened again with `AngularVelocityComponent` missing from `ApplyForcesSystem`'s query |
| 3 | Measure what `PhysicsScene`'s world arena needs | `kWorldArenaBytes` went from 16 KB to 256 KB in 4c without a measurement |
| 4 | Remove `AssetStore::CreateMesh`'s `std::cout` mesh counts | Leftover debug print |
| 5 | One `EcsTests` check through a `const World&` | The const `World::GetComponent` has no caller, so no build compiles its body. A scratch compile on 2026-10-27 showed it works |
| 6 | A named local per cube orientation in `Scene1` | It builds both in one long `?:` inside the initializer |
| 7 | A forward declaration instead of `Buffer.h` in `MaterialDescriptors.h` | The include is not needed |
| 8 | Remove the four spaces before `#version` in `phong.vert` | Tidiness |
| 9 | Narrow MemoryMonitor's logging to entity creation | Logging covers a scene's first `OnCreate`, asset loads included, so the OBJ parse floods the log with about 19,900 lines |
| 10 | Rename `fragWorldColor` to `fragColor` in `debug.vert` and `debug.frag`, and comment their inputs and outputs | The colour is not in world space, and `phong.vert` comments each one |
| 11 | A `reserve` in `LoadObjFile` | It grows its vectors and map one item at a time. Not measured; the load runs once per program because of the cache |
