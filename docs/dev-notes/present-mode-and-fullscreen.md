# Present mode as a setting, switched at runtime, and fullscreen

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed 2026-11-05 and built 2026-11-06, as steps 2, 3 and 4 of the frame pacing plan, which ended
on 2026-11-11 (commit `64964fc`). It came from ROADMAP.md, Rendering, "Frame pacing and the present
mode", and from the pacing prototype (`findings/2026-11-pacing-prototype.md`). "Step N" below means a step of that plan; README.md maps each one to its note.
Each section below is the step's whole plan section.

The plan's goal: the display paces the loop, not a hand-written sleep. The player picks the present
mode and a frame cap, and the engine shows what each choice costs. frame-cap-and-timer.md and
live-fps-and-pacing-scene.md hold the other steps.

## How games combine the present mode and the frame cap

From memory, not measured. A measuring step was dropped, because no result would change the menu or
the FIFO default.

| combination | what you get | who uses it |
|---|---|---|
| FIFO, no cap | fps matches the refresh rate, no tearing, a few frames of lag | the default in most games |
| FIFO + a cap that divides the refresh rate (120 on 240) | a steady lower rate, less power | consoles (30/60), laptops |
| FIFO + refresh minus 3, on a VRR screen | no tearing, and the lowest lag without tearing | competitive players with G-Sync or FreeSync |
| IMMEDIATE, no cap | the highest fps, tearing, the lowest lag | esports, "vsync off" |
| IMMEDIATE + a cap | tearing, less heat and power | "vsync off, limit 144" |
| MAILBOX, no cap | no tearing, low lag, the GPU runs flat out | NVIDIA Fast Sync, AMD Enhanced Sync |
| FIFO_RELAXED | vsync until a frame is late, then it tears | "adaptive vsync" |

## Step 2: Present mode as a setting

The swapchain picks the present mode on its own, inside the Vulkan code. The engine can't pick a
mode, can't ask which modes the GPU offers, and doesn't log what it got.

No graphics API offers every mode. OpenGL has a swap interval (1, -1 for adaptive vsync, 0) and no
MAILBOX; SDL_GPU has VSYNC, IMMEDIATE and MAILBOX, and only VSYNC is always supported (checked in
SDL's headers). DXGI and wgpu are from memory. FIFO is the one mode every API has, so the engine
asks which modes exist and never assumes. SDL_GPU and wgpu put the same kind of enum over several
backends.

**Decisions**

1. **The enum lives in `include/core/PresentMode.h`.** `core` is the layer both sides may see (rule
   1), so `Application` names a mode and the Vulkan backend maps it. Rejected: `include/vulkan/`,
   which `Application` could not include; `Window.h`, because the window does not present frames.
2. **It lists `Fifo`, `FifoRelaxed`, `Mailbox`, `Immediate`.** The four core Vulkan modes, each with a
   behaviour you can see. Rejected (names from memory): the two shared modes, which need an
   extension and serve VR and pen input; `FIFO_LATEST_READY`, an extension with no caller; an
   "Auto" entry, which the fallback covers.
3. **The renderer takes the starting mode in its constructor, and answers two queries:**
   `GetPresentMode()`, the mode in use, and `SupportsPresentMode(PresentMode)`, as SDL_GPU's
   `SDL_WindowSupportsGPUPresentMode` does. The mapping to `VkPresentModeKHR` stays in the Vulkan
   `.cpp`. `SetPresentMode` waits for step 3, its first caller; it adds about 15 lines and undoes
   nothing. Rejected: returning a list of modes, which allocates and still needs a search.
4. **A mode the GPU lacks falls back to FIFO,** and `GetPresentMode` then returns `Fifo`. A saved
   setting from another PC must still start. Rejected: throwing, which would crash on that setting;
   the "nearest" mode, which could bring tearing nobody asked for.
5. **The starting mode is a constant in `Application.cpp`,** `kStartPresentMode = PresentMode::Fifo`.
   The engine has no settings system yet. Rejected: a command-line argument, because step 3's key
   covers measuring; a settings file, which is a feature of its own.
6. **`Application` prints one line at startup:** `present modes: FIFO FIFO_RELAXED MAILBOX IMMEDIATE.
   Using FIFO`. It loops over the enum, asks `SupportsPresentMode`, then `GetPresentMode`.
   `PresentModeName(PresentMode)` sits beside the enum. A fallback shows as "Using FIFO". Rejected:
   printing from `Swapchain`, which would print on every resize, with Vulkan names.
7. **The check is a run on each GPU.** Rejected: a unit test for `PresentModeName`, which would only
   repeat a four-case `switch`.

| run | expected log |
|---|---|
| 4060 | `present modes: FIFO FIFO_RELAXED MAILBOX IMMEDIATE. Using FIFO` |
| 610M | no MAILBOX in the list, as the space prototype found. `Using FIFO` |
| 610M, `kStartPresentMode` set to `Mailbox` for one run | `Using FIFO`. The constant goes back afterwards |

Each run also passes the usual checks: no `[WARNING]`, no `[ERROR]`, 0 leaked blocks in Debug, and
`scripts\run-tests.bat` in all four configurations.

**What the build checked**

Scene0, windowed, `run-game.ps1 -Scene 0 -Steps "wait 3000"`. The 610M runs set
`VK_LOADER_DRIVERS_SELECT=*amd*` for that run only, so the Vulkan loader hides the NVIDIA driver and
the engine's first suitable GPU is the 610M.

| run | log | fps (profiler) |
|---|---|---|
| 4060, Release, `Fifo` | `present modes: FIFO FIFO_RELAXED MAILBOX IMMEDIATE. Using FIFO` | |
| 4060, Debug, `Fifo` | the same line | |
| 610M, Release, `Fifo` | `present modes: FIFO FIFO_RELAXED IMMEDIATE. Using FIFO` | |
| 610M, Release, `Mailbox` | `present modes: FIFO FIFO_RELAXED IMMEDIATE. Using FIFO`: the fallback | 234.8 |
| 4060, Release, `Mailbox` | `present modes: FIFO FIFO_RELAXED MAILBOX IMMEDIATE. Using MAILBOX` | 5,351.6 |

- MAILBOX on the 4060 ran Scene0 at 5,351.6 fps. The prototype's pacing scene read 479.7 in MAILBOX.
  One run each, different scenes; step 3's runtime switch can compare them in one run.
- Every run: exit code 0, no `[WARNING]`, no `[ERROR]`, no asserts, no compiler warnings.
- Debug: 0 leaked blocks.
- `scripts\run-tests.bat`: all four configurations pass.

## Step 3: Switch the mode at runtime

The mode is fixed at startup, so comparing modes means editing the constant and rebuilding. A key
lets you compare them side by side, and lets `run-game.ps1` measure each one in a single run.

**Decisions**

1. **Ctrl+P, handled in `Application::HandleEvent`** beside F1 to F4 and G. It calls
   `renderer.SetPresentMode(next)`, so a future ImGui menu calls the same function. A modifier keeps
   these keys apart from scene keys; SDL puts the held modifiers in `event.key.mod`. Later settings
   follow the same pattern (Ctrl+L for the cap). Rejected: Alt+P, because Alt+Enter is taken and
   Windows gives Alt a menu meaning; Shift+P, because the fly camera uses Shift.
2. **Ctrl+P moves to the next mode in enum order,** FIFO → FIFO_RELAXED → MAILBOX → IMMEDIATE → FIFO,
   skipping any mode `SupportsPresentMode` rejects. Every press changes something. Rejected: one key
   per mode, which is four keys to remember.
3. **The rebuild happens at the start of the next frame, through the resize path.** `SetPresentMode`
   stores the wanted mode and sets `swapchainNeedsRebuild`. One path, run where no frame is half
   recorded. Rejected: rebuilding inside `SetPresentMode`, a second path that a menu could call
   mid-frame.
4. **The rebuild stays as it is: `vkDeviceWaitIdle`, destroy, create.** A switch is rare, and the
   stall is one frame (a guess until the check measures it). Rejected: passing `oldSwapchain`. The
   old swapchain can only be destroyed once the screen lets go of its images, which core Vulkan
   can't report; that needs `VK_EXT_swapchain_maintenance1` (from memory) and extra tracking.
5. **`Application` prints `present mode: MAILBOX` when Ctrl+P is pressed,** with the mode it asked
   for. The cycle only picks supported modes, so that is the mode the rebuild uses. `GetPresentMode`
   returns the old mode until the next frame, and its comment says so. Rejected: the renderer
   printing after the rebuild, which puts printing in the backend.
6. **The check is a Debug run, for the validation layers.** Rejected: Release only, where a bad
   rebuild shows no validation errors.
   1. Press Ctrl+P five times, about 1.5 s apart (`hold Ctrl,P 100` in `run-game.ps1`).
   2. Expected log: `FIFO_RELAXED`, `MAILBOX`, `IMMEDIATE`, `FIFO`, `FIFO_RELAXED`. On the 610M,
      no `MAILBOX`.
   3. No `[WARNING]`, no `[ERROR]`, 0 leaked blocks.
   4. The profiler's max frame at exit gives the cost of the stall in decision 4.

   Plus `scripts\run-tests.bat` in all four configurations.

`run-game.ps1` gained Ctrl, Alt and Enter for this step and step 4. Tested 2026-11-06 on the
prototype: `hold Ctrl,P 100` switched the mode twice, a plain P did nothing, and `hold Alt,Enter 100`
went fullscreen and back. Debug, no `[WARNING]`, no `[ERROR]`, 0 leaked blocks.

**What the build checked**

Debug, Scene0, windowed. `run-game.ps1 -Config Debug -Steps "wait 1500"`, then five times
`"hold Ctrl,P 100","wait 1500"`. `kStartPresentMode` was still `Mailbox` on the 4060, so its cycle
starts one mode later than the expected log above.

| GPU | start | log after the five presses |
|---|---|---|
| 4060 | MAILBOX | IMMEDIATE, FIFO, FIFO_RELAXED, MAILBOX, IMMEDIATE |
| 610M (`VK_LOADER_DRIVERS_SELECT=*amd*`) | FIFO, the fallback | FIFO_RELAXED, IMMEDIATE, FIFO, FIFO_RELAXED, IMMEDIATE: no MAILBOX |

- Decision 4's stall, from the profiler's frame dumps (frames over 30 ms). Each switch makes one long
  frame, nearly all of it in `Render`. 4060: 165.9, 74.0, 80.5 and 54.6 ms; one switch stayed under
  30 ms. 610M: up to 157.9 ms. Debug with validation on, one run, so Release will be shorter.
- 4060: no `[WARNING]`, no `[ERROR]`, no asserts, 0 leaked blocks.
- 610M: no `[ERROR]`, no asserts, 0 leaked blocks. One `[WARNING]` from the Vulkan loader:
  `Driver "nv-vk64.json" ignored because not selected by env var`. The test's own environment
  variable causes it, not the engine.
- `scripts\run-tests.bat`: all four configurations pass.
- A first build printed IMMEDIATE on every press: `SetPresentMode` stored the wanted mode but never
  raised the rebuild flag, so the mode in use never changed.

## Step 4: Fullscreen with Alt+Enter

A normal window shares the screen with the Windows compositor, which can hide tearing, so seeing
tearing needs fullscreen. Alt+Enter is the usual key in games. SDL does not add it on its own.

**Decisions**

1. **Borderless fullscreen:** `SDL_SetWindowFullscreen(window, true)` with no display mode set, at the
   desktop's resolution. On Windows 10 and 11 a borderless window that covers the screen can skip the
   compositor ("independent flip") and tear like exclusive mode (from memory, not measured).
   Rejected: exclusive fullscreen, which flickers on the mode change, slows Alt+Tab and can upset
   other monitors.
2. **`Window` owns it:** `Window::SetFullscreen(bool)` and `Window::IsFullscreen()`. Fullscreen is a
   property of the OS window. `Application` catches the key and calls them, and a future menu calls
   the same two. Rejected: `Application` calling SDL through `GetSDLWindow()`, which spreads SDL window
   calls; the renderer, which only reacts to the new size.
3. **The renderer also hears about the resize from SDL.** `Application` sees
   `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` and asks the renderer for a rebuild, which sets the same
   `swapchainNeedsRebuild` flag as step 3. A second trigger in one frame costs nothing. Rejected:
   trusting `OUT_OF_DATE` and `SUBOPTIMAL` alone; Windows drivers report them, but the spec does not
   promise it.
4. **The toggle runs when the key is Enter or keypad Enter, Alt is held, and the press is not a
   repeat.** `HandleEvent` returns after it, so the scene's `HandleEvents` never sees that Enter.
   Rejected: keeping the prototype's F11 as well; one key per action until a menu exists.
5. **App keys stay on SDL events.** F1 to F4, G, Ctrl+P and Alt+Enter all read `SDL_Event`.
   Known leak: `HandleEvent` writes every key into `InputState.pressed` before it checks for its own,
   so systems still see app keys. An action-mapping layer (like Unity's Input System or Unreal's
   Enhanced Input, from memory) would fix that, and all the app keys would move to it together.
6. **The game starts windowed at 1280×720.** SDL restores the window's size and position when
   fullscreen ends (from memory; the check confirms it). Rejected: starting fullscreen, which breaks
   the script's `resize` and `click` steps; a command-line flag, which has no caller.
7. **`Application` prints `fullscreen: on` or `fullscreen: off` on each toggle,** as step 3 prints the
   mode.
8. **The check is a Debug run, for the validation layers.**
   1. `hold Alt,Enter 100`, wait, `shot`. Expected: the display's full size (1920×1200 on the laptop).
   2. `hold Ctrl,P 100` in fullscreen. Expected: the mode switches with no errors.
   3. `hold Alt,Enter 100`, wait, `shot`. Expected: back to 1280×720.
   4. `press Enter` with no Alt. Expected: nothing changes.
   5. No `[WARNING]`, no `[ERROR]`, 0 leaked blocks.

   By hand: the keypad Enter, and listening for the Windows "ding" on Alt+Enter.
   Rejected: screenshots alone, which miss a bad rebuild.

**What the build checked**

Debug, Scene0, the 4060. `run-game.ps1 -Config Debug -Steps "wait 1500","hold Alt,Enter 100",
"wait 2000","shot full","hold Ctrl,P 100","wait 1500","hold Alt,Enter 100","wait 2000",
"shot windowed","press Enter","wait 1500","shot enter"`.

| step | log | screenshot |
|---|---|---|
| Alt+Enter | `Fullscreen: 1` | 1920×1080, the whole display, cubes at the right aspect |
| Ctrl+P in fullscreen | `present mode: FIFO_RELAXED` | |
| Alt+Enter | `Fullscreen: 0` | 1296×759: the 1280×720 window with its border, where it was |
| Enter, no Alt | nothing | 1296×759, unchanged |

- The display is 1920×1080, not the 1920×1200 the check above guessed.
- No `[WARNING]`, no `[ERROR]`, no asserts, 0 leaked blocks, no compiler warnings.
- Keypad Enter was added before the commit (`64964fc`), with the `fullscreen: on` and `off` print and a
  `[WARNING]` when `SDL_SetWindowFullscreen` fails. The Windows "ding" was not checked.
