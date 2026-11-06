# Scene switcher: scenes load their own assets, and `main` shrinks

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed 2026-10-20, built 2026-10-21, as step 1 of the plan that ended on 2026-11-03.

## Sources

- Dependency inversion, for the `ResourceLoader` interface.
- `2026_GPUProgramming`'s `SceneManager`, the old course project, as the thing to improve on.

## What it does

The program starts in any scene and switches scenes with a key. `GameEngines5.exe 0` opens Scene0.
No argument opens the last scene, and a number outside the table prints the list and exits with an
error. F1 builds scene 0, F2 scene 1, and so on. The current scene's key restarts it. A key past the
table does nothing.

Each scene loads its own assets through a `ResourceLoader`, and a cache makes a repeat visit load
nothing. `main` holds only the memory baseline, the error catches, `Application` and the closing
reports.

## The pieces

**`GetWorld` and the report.** `Scene` gained a const `GetWorld`. The ECS report became
`PrintEcsReport(const World&)`, so it works for any scene. The profiler label `Scene0::Update`
became `Scene::Update`. Each `World` owns its arena, so the world arena lines moved into the ECS
report, which prints on every switch. The frame arena serves every scene, so its report stays
separate and prints once, at the end.

**`ResourceLoader` and the cache.**

- `include/core/ResourceLoader.h` is an interface with four functions: `CreateCube`, `CreateMesh`,
  `CreateTexture` and `CreateMaterial`. It holds no Vulkan type. `ResourceStore` inherits it
  directly. It replaced the empty `include/renderer/Renderer.h`.
- The problem: a scene must load its own assets, and core and scene code may not include Vulkan
  headers. The scene and the store both depend on the contract in the middle. It is not an Adapter,
  because nothing wraps the store.
- The cache lives in `ResourceStore`: one optional handle for the cube, a map from path to handle
  for meshes and for textures, and a short list of material descriptions searched with a loop. Each
  `Create` function checks its cache first, and loads only on a miss.
- The key is the path exactly as written, so `"./assets/..."` and `"assets/..."` count as two files.
  Materials compare their three values exactly.
- The material cache is needed because the material pool holds 64 sets. Scene1 makes two
  materials, so about 30 switches without a cache would fill the pool and throw.
- Nothing is freed on a switch. The store grows to the sum of every asset visited, then stops.
- File I/O will not replace the interface. A scene-file reader becomes a new caller of it.

**Scenes load their own assets.**

- `OnCreate` takes the loader. The scene asks for its meshes and materials, keeps the handles, then
  creates its entities. It borrows the loader for one call and keeps no reference.
- The scene constructors lost their handle parameters: `Scene1()` instead of
  `Scene1(marioMesh, shinyMario, cube, matteSkull)`.
- `OnCreate` runs on every visit, because a switch destroys the scene object and builds a new one.
  Each visit starts fresh, and the cache makes a second visit cost only entity creation.

**`SceneManager` handles scene lifetimes only.**

- It holds a table of `SceneEntry` rows: a name and a plain function pointer that builds the scene,
  filled with a lambda that captures nothing. The table stores how to build each scene, so a scene
  exists only while it is in use. Adding a scene is one row.
- The manager holds the table as a `std::span`, so the table must outlive it.
- `GetCurrentScene()` returns `Scene&`. A caller must not keep the reference across a switch.
- `ApplyPendingSwitch` runs at the top of a frame. It clears the pending switch, then calls `Start`.
  Clearing first means a scene that throws in `OnCreate` is not retried every frame.
- `Start` closes the running scene before it builds the next, so two `World`s never hold memory at
  once.
- A private `CloseCurrent()` runs `OnDestroy`, then the ECS report, then destroys the scene.
  `OnDestroy` goes first because it folds the last frame into the totals. `Start` and the destructor
  both call it, so no path skips `OnDestroy`.
- A switch is safe for the GPU: `DrawFrame` copies the camera and lights and records the draw list
  before it returns, so nothing on the GPU points into a scene.

**`Application` holds the rest of `main`.**

- Its members, in declaration order: `Window`, `VulkanRenderer`, `SceneManager`, `Timer`, the frame
  `Arena` and its `ArenaResource`. C++ destroys them in reverse, so the scenes go before the
  renderer.
- It lives in `include/app` and `src/app`, not `core`, because it holds a `VulkanRenderer` by value
  and core may not include Vulkan headers. Only `main` includes `app`.
- The scene table and `PickScene` sit in `Application.cpp` at file scope. `PickScene` reads
  `argv[1]` with `std::from_chars` and rejects text, trailing characters and indices past the table.
- `Run()` holds the loop: events, the pending switch, `Update`, the draw list, `DrawFrame`, the wait
  and the profiler's frame markers. It prints the frame arena report at the end.
- The last scene's ECS report comes from the `SceneManager` destructor, not from `Run`. That also
  covers an exception thrown in the middle of `Run`.
- `HandleEvent` takes one event and handles only F1 to F4. Every other event goes to the scene, and
  quit stays with `Window::ShouldClose`. Number keys stay with Scene1's lighting.
- `main` runs the profiler, memory and leak reports after `Application` is destroyed. Otherwise the
  leak report would count the window, the renderer and the scenes.
- Why two classes: each has one reason to change. Scene switching changes `SceneManager`. A new
  subsystem, such as audio, changes `Application`.

Step 3 later made `ApplyPendingSwitch` return `bool` and had `Start` print each scene's systems: see
systems-and-phases.md.

## Rejected

- Keeping every scene alive and pausing it: it keeps each scene's state, but every `World` stays in
  memory, and each scene needs pause and resume functions.
- `std::function` for a table row: it can allocate, which adds noise to MemoryMonitor's log.

## Compared with `2026_GPUProgramming`

Its `SceneManager` mixes both jobs. Adding a scene there takes three edits: an enum, a `switch` and
the key `switch`. It uses raw `new` and `delete`, deletes the old scene without `OnDestroy`, rebuilds
Scene0 on any key not listed, calls `OnCreate` on a null pointer for the unfinished scenes, ignores a
failed `OnCreate`, and hands the switch key to the new scene. Its scenes receive the whole
`Renderer*`; ours receive only the loader.

## What the build found

- The assets now load while MemoryMonitor's logging is on, so a first visit logs about 19,900 `new`
  and `delete` lines. Most come from tinyobjloader parsing Mario (a guess from how each library
  allocates, not split per call).
- `ApplyPendingSwitch` runs before the `ShouldClose` check, so a switch pressed in the closing frame
  builds a scene and destroys it at once. Harmless, and left as it is.
- A bad argument exits with code 1 from `main`'s `catch`, which returns before the leak report. That
  was already true before this step.

## Measured

- A test asked for every asset twice. All four kinds returned the same handle, a material with new
  values got a new handle, and 100 repeat material requests did not fill the pool.
- A script pressed F1 and F2 in turn in the running program. 20 switches and 60 switches both gave
  0 leaked blocks, no validation output, and the same peak of 592,775 live bytes when starting in
  Scene0 (592,567 when starting in Scene1).
- A first visit's `OnCreate` took about 128 ms in Debug. A repeat visit took 0.066 ms, because the
  cache loads nothing.
