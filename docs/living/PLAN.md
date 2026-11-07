# Plan

**Last updated:** 2026-11-06.

Frame pacing and the present mode. The display paces the loop, not a hand-written sleep. The player
picks the present mode and a frame cap, and the engine shows what each choice costs. From the
ROADMAP.md block "Frame pacing and the present mode", and from the prototype in
`findings/2026-11-pacing-prototype.md`.

## Steps

| # | step | done when | stage |
|---|---|---|---|
| 1 | `Timer` counts nanoseconds | at 240 fps the time step averages about 4.17 ms, and no frame reads a whole millisecond | designed |
| 2 | The present mode as a setting, with the modes the GPU offers logged at startup. Step 1 already made FIFO the default | the log lists the modes on the 4060 and the 610M | designed |
| 3 | A key switches the present mode while the game runs, and the swapchain rebuilds | Ctrl+P cycles every mode the GPU offers, with no validation errors | designed |
| 4 | Alt+Enter switches borderless fullscreen on and off | the window fills the screen and comes back, and the swapchain rebuilds with no validation errors | designed |
| 5 | The frame cap as a value beside the window, with a precise sleep. Step 1 deleted `GetSleepTime` | cap 60 reads 16.67 ms on every frame | designed |
| 6 | The fps and frame times shown live | the window title and the console show them | designed |
| 7 | A pacing scene: two rows of Marios that move fast and wrap around, one row moved per frame and one per fixed step | tearing and judder are visible by eye | designed |

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

**For this plan's dev-note:** the table below goes into it. It describes how games combine the two
settings, from memory, not measured. A measuring step was dropped, because no result would change
the menu or the FIFO default.

| combination | what you get | who uses it |
|---|---|---|
| FIFO, no cap | fps matches the refresh rate, no tearing, a few frames of lag | the default in most games |
| FIFO + a cap that divides the refresh rate (120 on 240) | a steady lower rate, less power | consoles (30/60), laptops |
| FIFO + refresh minus 3, on a VRR screen | no tearing, and the lowest lag without tearing | competitive players with G-Sync or FreeSync |
| IMMEDIATE, no cap | the highest fps, tearing, the lowest lag | esports, "vsync off" |
| IMMEDIATE + a cap | tearing, less heat and power | "vsync off, limit 144" |
| MAILBOX, no cap | no tearing, low lag, the GPU runs flat out | NVIDIA Fast Sync, AMD Enhanced Sync |
| FIFO_RELAXED | vsync until a frame is late, then it tears | "adaptive vsync" |

---

## 1. Timer in nanoseconds

`Timer` reads `SDL_GetTicks`, which counts whole milliseconds. At 240 fps a frame takes 4.17 ms, so
the time step jumps between 4 ms and 5 ms, and anything moved by it jitters. The prototype found
this.

**Decisions**

1. **The clock is `SDL_GetTicksNS`.** Step 5's sleep is `SDL_DelayNS`, so the timer and the cap read
   one clock. `SDL_GetTicks` costs the same: both read `QueryPerformanceCounter`, then multiply and
   divide (`SDL_timer.c`). Rejected: `std::chrono::steady_clock`. It reads the same counter on MSVC,
   but it adds a second clock API to the loop. The profiler keeps it, because it never sleeps.
2. **`Timer` stores `std::uint64_t` nanoseconds, and `GetDeltaTime` returns `float` seconds.** The
   integers subtract exactly, and only the small difference becomes a `float`. Every caller already
   takes `float` seconds. Unity, Unreal and Bevy also use a `float` time step and keep total time in
   a `double` or integer ticks (from memory). Rejected: a `double` time step, which would change
   `SystemContext`, `FixedStep` and every system for no gain.
3. **`GetCurrentTicks` is deleted.** Nothing calls it, and it returns the total time as a `float`.
   It comes back as integer ticks or a `double` when a caller needs it.
4. **`GetSleepTime` and the `SDL_Delay` line are deleted, and `ChooseSwapPresentMode` returns
   FIFO.** The display paces the loop, so the check can see 240 fps. Vulkan guarantees FIFO, and
   physics runs on the fixed step. Step 2 keeps the enum, the setting and the log; the FIFO default
   is done here. Side effect: the profiler's frame total reads the refresh rate, about 4.17 ms,
   until step 3 can switch to MAILBOX. Rejected: keeping `GetSleepTime` until step 5. The 60 cap
   would hide the fix.
5. **The check is a run, not a unit test.** A unit test would need a fake clock passed into `Timer`,
   and only the test would use it. A temporary line prints the shortest and longest `GetDeltaTime`
   of the run, the frame number of the longest, and how many frames fall outside 3.9 to 4.4 ms. It
   skips the first 60 frames, and it comes out before the commit.

**Build order**

1. Delete the sleep and switch to FIFO. Run Scene0 in Release for about 5 s with the temporary
   print. Expected, deduced from the code: only 4 ms and 5 ms.
2. Switch `Timer` to nanoseconds. Run again. Expected: an average of about 4.17 ms, and no frame
   at a whole millisecond. The first design expected every frame between 3.9 and 4.4 ms; the runs
   below showed that a CPU timestamp can't promise that.

Both runs also pass the usual checks: no `[WARNING]`, no `[ERROR]`, 0 leaked blocks in Debug, and
`scripts\run-tests.bat` in all four configurations.

**What the build checked**

All runs on the 4060 and the 240 Hz panel, windowed, Scene0, `run-game.ps1 -Scene 0 -Steps "wait 5000"`.

| run | min | max | outside 3.9–4.4 ms | average (profiler) |
|---|---|---|---|---|
| Release, ms timer, FIFO, no sleep | 0 ms | 9 ms | not counted | |
| Release, ms timer, second run | 0 ms | 13 ms | not counted | |
| Release, ns timer, run 1 | 0.823 ms | 7.568 ms, frame 1285 of 1976 | 413 | 4.186 ms, 238.9 fps |
| Release, ns timer, run 2 | 0.839 ms | 7.436 ms, frame 1358 of 1988 | 393 | 4.186 ms, 238.9 fps |
| Release, ns timer, run 3 | 0.902 ms | 7.129 ms, frame 845 of 1996 | 399 | 4.168 ms, 239.9 fps |

- With the ms timer every reading is a whole millisecond, as `SDL_GetTicks` predicts. With the ns
  timer none is.
- About 1 frame in 5 falls outside 3.9 to 4.4 ms, and the longest frame lands mid-run, not at the
  start or the close. The average stays at the refresh rate, so the screen keeps a steady 240 Hz.
  A guess, not measured: `Timer` reads the clock when the CPU wakes from acquire or the fence, not
  when the screen shows the frame, and those wake times vary. Step 7's scene shows whether this
  uneven time step makes the top row stutter.
- Two earlier ns runs, before the count existed, read a max of 12.3 and 15.0 ms. The three runs
  above didn't repeat it.
- Every run: exit code 0, no `[WARNING]`, no `[ERROR]`, no asserts, 0 frame-arena requests fell to
  the heap.
- Debug, ns timer: no `[WARNING]`, no `[ERROR]`, 0 leaked blocks.
- `scripts\run-tests.bat`: all four configurations pass.

## 2. Present mode as a setting

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

## 3. Switch the mode at runtime

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

## 4. Fullscreen with Alt+Enter

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
- Not checked: keypad Enter, which the code doesn't test for yet, and the Windows "ding", by hand.

## 5. Frame cap with a precise sleep

`GetSleepTime` has three defects: it divides whole milliseconds, so 60 means 62.5; any cap above 1000
means no cap; and `SDL_Delay(0)` still costs about 0.48 ms. The prototype's cap sleeps with
`SDL_DelayNS` to 1 ms short of the frame's end, then spins, and held 60 fps to 0.01 ms.

**Decisions**

1. **A `FrameCap` class in `include/core/`,** owned by `Application` beside `window` and `timer`.
   Fields: `double framesPerSecond`, `uint64_t budgetNanoseconds`, `uint64_t frameEndNanoseconds`.
   Functions: `SetFramesPerSecond(double)`, where 0 means off, and `Wait()`. Rejected: putting it in
   `Timer`, which would give `Timer` two jobs; a free function, which has nowhere to keep the deadline.
2. **`Wait()` sleeps with `SDL_DelayNS` until 1 ms before the deadline, then spins on
   `SDL_GetTicksNS`.** The 1 ms is a named constant, `kSpinNanoseconds`. On Windows `SDL_DelayNS` uses
   a high-resolution waitable timer (`src/timer/windows/SDL_systimer.c:106`), but it can still wake
   late. Rejected: sleeping alone, which wakes late; spinning alone, which burns a core.
3. **Fixed deadlines:** each frame adds the budget to the last deadline. A frame late by less than one
   budget keeps the grid. A frame late by more restarts the grid from now. Rejected: "budget minus
   this frame's time", which drifts; catching up, which sends a burst of short frames.
4. **The wait stays at the end of the loop,** right before the next frame reads input, where
   `SDL_Delay` was. The `Wait` profiler span stays. Rejected: waiting before input to cut lag (the
   NVIDIA Reflex idea); the FIFO queue adds more lag than this, and it's out of scope.
5. **Ctrl+L cycles off → 30 → 60 → 100 → half the refresh rate → refresh rate minus 3 → off,** and
   prints `frame cap: 60` or `frame cap: off`.
   - 100 divides neither 240 nor 165, so step 7's scene can show the judder.
   - Refresh minus 3 is for VRR screens (G-Sync, FreeSync). Just under the refresh rate, the screen
     shows each frame as it arrives, and FIFO never queues. On a screen without VRR, 3 frames a second
     miss their refresh and stutter. The check tests which one each screen does.
   - The refresh rate comes from a new `Window::GetRefreshRate()`, which wraps
     `SDL_GetCurrentDisplayMode` for the window's display and returns 60 when SDL doesn't know.
     `Application` asks at each press, because the window can move between the 240 Hz and 165 Hz
     screens.

   Rejected: the prototype's 144, which shows nothing that 100 doesn't.
6. **The cap and the present mode are independent.** No code links them. In FIFO with a cap above
   the refresh rate, FIFO already blocks, so `Wait()` finds the deadline passed and returns at once.
   In MAILBOX or IMMEDIATE the cap is the only limit, and it saves power. Most PC games offer the two
   as separate settings for the same reason (from memory). Rejected: clamping the cap to the refresh
   rate, or turning it off in FIFO; that's code for a case that already does nothing.
7. **The check is a set of Release runs.** Step 6's live fps doesn't exist yet, so a temporary line
   prints each second's shortest and longest frame, as in step 1. It comes out before the commit.

   | run | expected |
   |---|---|
   | FIFO, cap 60 | every frame 16.67 ms, min and max within 0.05 ms |
   | FIFO, cap 100 on 240 Hz | 100 fps on average |
   | MAILBOX, cap 60 | 60 fps: the cap works without FIFO |
   | FIFO, cap 60, `kSpinNanoseconds` set to 0 for one run | how late the timer really wakes. That decides the 1 ms margin. The constant goes back afterwards |
   | FIFO, refresh minus 3, on each screen | smooth on a VRR screen, 3 stutters a second without VRR |

   Plus a Debug run (no `[WARNING]`, no `[ERROR]`, 0 leaked blocks) and `run-tests.bat` in all four
   configurations. Rejected: a unit test of the deadline math, which needs a fake clock passed in, the
   same reason as step 1.

**What the build checked**

Not built yet.

## 6. Live fps

The engine has no text. The window title costs a few lines. It can't be seen in fullscreen, so the
console prints the same line once a second.

**Decisions**

1. **A `FrameStats` class in `include/core/`,** owned by `Application`. Its one input is
   `AddFrame(uint64_t nanoseconds)`, fed with the frame time `Timer` measured in step 1. It never reads
   a clock, so a unit test can feed it fixed frame times. Rejected: the prototype's loose fields and
   `UpdateStats` on `Application`, which give `Application` a second job; building it on the profiler,
   whose job is span timings in a dump, compiled for profiling runs only.
2. **Four numbers per window:** the frame count, the total time, the shortest frame and the longest
   frame. The report gives fps (frames divided by total time), the average frame in ms, and the
   shortest and longest. The longest frame catches a stutter that an average hides. Rejected: "1% low"
   or percentiles, which need every frame time stored and sorted; tools like PresentMon give those from outside the engine.
   Averaging 1/frame time, which a few short frames pull up.
3. **One window of 1 second** feeds both the title and the console line, so every frame lands in
   exactly one printed line. Rejected: the prototype's 0.25 s title and 1 s console, where the console
   printed only every fourth window and missed the longest frame of the other three; setting the title
   every frame, an OS call that would cost more than the frame at 3,000 fps.
4. **`AddFrame` returns `true` when the summed frame times reach 1 second.** `Application` then reads
   a `FrameReport` struct (`fps`, `averageMs`, `shortestMs`, `longestMs`), and the counts reset. A new
   `Window::SetTitle(const std::string&)` sets the title. Rejected: `Application` calling
   `SDL_SetWindowTitle` through `GetSDLWindow()`, which spreads SDL window calls, as in step 4.
5. **One line for both the title and the console:**
   `FIFO | cap off | 239.8 fps | 4.17 ms (min 4.10, max 4.25)`. The mode and the cap come first, so a
   screenshot or a log line records the settings beside the numbers. Rejected:
   numbers only; the prototype's load and Hz fields, since the load belongs to step 7's scene and the
   Hz rarely changes.
6. **No switch for the console line.** It prints once a second in every configuration, so every
   `run-game.ps1` run records its frame rate in `out.txt`. Rejected: a toggle key, which nobody needs
   yet; a future ImGui menu is the place for it.
7. **The check:**
   1. Unit tests for `FrameStats`:
      - 240 frames of 4,166,667 ns: `AddFrame` returns `true` on the last one, with fps 240.0,
        average 4.17 ms, and min and max 4.17.
      - One 50 ms frame among short ones: max reads 50.00, and the average stays near the short
        frames.
      - After a report, the next second starts from zero.
   2. A Release run with `run-game.ps1`:
      - FIFO, no cap: about 240 fps in `out.txt`.
      - `hold Ctrl,L 100` to cap 30, then cap 60: 30.0 and 60.0 fps, with min and max at 33.33 and
        16.67.
      - `hold Alt,Enter 100`: the console lines keep coming in fullscreen.
      - A `shot` of the window: the title shows the line.
   3. Step 5's temporary min/max print gets deleted. This line replaces it.
   4. A Debug run (no `[WARNING]`, no `[ERROR]`, 0 leaked blocks) and `run-tests.bat` in all four
      configurations.

**What the build checked**

Not built yet.

## 7. Pacing scene

A test scene to see tearing and judder by eye. The prototype's scene
used two sweeping bars of cubes. This one uses Marios, so it tests the OBJ path too.

**Decisions**

1. **A new `PacingScene`, written fresh, at row 3 of `kScenes`** ("Pacing Scene"). It has two rows of
   10 Marios from `assets/meshes/Mario.obj`, loaded the way Scene1 and PhysicsScene load it. The top
   row moves in the Update phase, once per frame. The bottom row moves in the Fixed phase, 60 times a
   second. The scene is new, so the check measures its own numbers. Rejected: copying the prototype's
   cube scene, which skips the OBJ path; putting the rows into Scene0, whose 1,000 cubes would change
   the frame times.
2. **The motion wraps around.** Each Mario moves right with `x += speed * deltaTime`. Past the right
   edge, the code subtracts the path's full width, so the leftover distance carries over. The 10
   Marios in a row are spaced evenly along the path, so the row always fills the screen. The path is
   wider than the camera's view, so the jump back happens off screen. Rejected: resetting `x` to the
   start, which drops the leftover, so the rows drift apart and the speed reads wrong; wrapping at the
   edge of the view, where the pop looks like a pacing bug.
3. **The fake load is a `LoadSystem` in the scene's Update phase.** It spins on `SDL_GetTicksNS` for
   the chosen time, so it works the CPU like real work, and the profiler shows it as its own span.
   Rejected: the prototype's `AddLoad` in `Application`, which puts one scene's test feature in every
   scene's loop; sleeping, which lets the CPU rest and can wake late.
4. **Ctrl+H, in the scene's `HandleEvents`, cycles the load:** off → 5 ms every frame → 7.5 ms every
   frame → a 25 ms spike every 30 frames → off. It prints `load: 5 ms every frame`. 5 ms is 1.2
   refreshes at 240 Hz, as in the prototype's "every frame late" run. 7.5 ms is late at 165 Hz too.
   The spike is a single stutter for step 6's max to catch. Rejected: the prototype's "1.25
   refreshes"; the scene can't see the refresh rate, and fixed numbers make runs repeatable.
5. **The bottom row stays.** It steps at 60 Hz at any frame rate, because nothing interpolates
   between fixed steps. That's the case for interpolation, which is out of scope here. Rejected:
   dropping it; it's the only place the engine shows that problem.
6. **The check:**
   1. A Release run of scene 3 with `run-game.ps1`. Measure and record each line, since Marios cost
      more to draw than the prototype's cubes:
      - No load, FIFO: the fps, and a `shot` showing both rows.
      - `hold Ctrl,H 100` once (5 ms): the fps in FIFO. The prototype's cubes read 190.
      - Twice more (the spike): the max near 25 ms in step 6's line, with the average close to the
        no-load run.
   2. By hand, since a script can't see motion:
      - IMMEDIATE: the top row tears, and whether the tear is easy to see on Mario's shape.
      - Cap 100: the top row judders.
      - The bottom row steps at any setting.
   3. A Debug run that switches into the scene and out again. No `[WARNING]`, no `[ERROR]`, 0 leaked
      blocks.

   Rejected: unit tests. The scene's point is what you see, and step 6's line checks the numbers.

**What the build checked**

Not built yet.
