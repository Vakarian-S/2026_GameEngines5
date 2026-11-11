# Frame cap and a nanosecond timer

> **Legacy doc.** Written when this feature was finished, and never updated since. The code may have changed, so parts of it may no longer be accurate.

Designed 2026-11-05 and built 2026-11-06 to 2026-11-11, as steps 1 and 5 of the frame pacing plan,
which ended on 2026-11-11 (step 1 in commit `64964fc`, step 5 in `392c61f`). It came from ROADMAP.md,
Rendering, "Frame pacing and the present mode", and from the pacing prototype
(`findings/2026-11-pacing-prototype.md`). "Step N" below
means a step of that plan; README.md maps each one to its note. Each section below is the step's
whole plan section.

The plan's goal: the display paces the loop, not a hand-written sleep. The player picks the present
mode and a frame cap, and the engine shows what each choice costs. present-mode-and-fullscreen.md and
live-fps-and-pacing-scene.md hold the other steps.

## Step 1: Timer in nanoseconds

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

## Step 5: Frame cap with a precise sleep

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

**Changes during the build**

- The spin loop calls `SDL_CPUPauseInstruction()`. Godot's Windows limiter (`OS_Windows::add_frame_delay`,
  `YieldProcessor()`) and Special K (`framerate.cpp`) put the same pause in their spins.
- `SDL_DelayPrecise` was tried as a third option beside sleep and spin. SDL's own comment says it never
  spins, on purpose, and accepts a small error. It lost on steadiness (table below).
- The profiler's frame dump fired on every frame over a fixed 30 ms, so at cap 30 it printed a table
  every frame, and the printing landed in the measured frame times. The threshold is now 1.5 cap
  budgets, never under 30 ms: 50 ms at cap 30, 30 ms with no cap or a cap of 60 and up.

**What the build checked**

Laptop panel, 165 Hz, the 4060, windowed, Scene0, Release unless noted. Ctrl+L steps through the caps
with `hold Ctrl,L 100`. A temporary line prints each second's frame count, shortest and longest frame;
the second each cap starts in is left out. One run per row.

| run | fps | frame min / max |
|---|---|---|
| FIFO, no cap | 165 | 5.6 / 6.6 ms |
| FIFO, cap 30 (old dump threshold) | 30–31 | 32.68 / 33.99 ms |
| FIFO, cap 30 (new threshold) | 30–31 | 33.32 / 33.36 ms |
| FIFO, cap 60 | 60–61 | 16.65 / 16.68 ms; one second 16.52 / 16.81 |
| FIFO, cap 100 | 100–101 | 9.99 / 10.01 ms |
| FIFO, cap 82.5 (half of 165) | 83 | 12.11 / 12.13 ms; one second 11.95 / 12.29 |
| FIFO, cap 162 (refresh − 3) | 153–163 | one steady second at 6.09 / 6.26, the others 0.15 to 23 ms |
| MAILBOX, cap 60 | 60–61 | 16.23 / 17.10 ms |

The wait, compared at cap 60 in FIFO. `FrameCap.cpp` was changed for each run and restored afterwards
(same MD5 hash):

| wait | cap 60 |
|---|---|
| sleep + 1 ms spin (the plan) | 16.58 / 16.75 ms, most seconds 16.65 / 16.68 |
| sleep only (`kSpinNanoseconds = 0`) | 15.99 / 17.53 ms |
| sleep + 2 ms spin | 16.62 / 16.71 ms |
| `SDL_DelayPrecise` | 16.09 / 17.25 ms; 5 of 6 seconds 16.58 / 16.76 |

- The sleep alone wakes up to about 0.9 ms late, so the 1 ms margin holds. 2 ms buys nothing.
- The old cap 30 wobble was the dump, not the sleep. The profiler's frame totals, which leave out the
  printing, never went past 33.32 ms with a spin. A 2 ms spin left the wobble as it was, and the new
  threshold removed it: 0 tables printed, 33.32 / 33.36 ms.
- Refresh − 3 stutters on the laptop panel. Whether the panel has VRR on is not known.
- MAILBOX ran at 165 fps uncapped on the laptop panel, against 5,351.6 fps on the 240 Hz monitor in
  step 2. A guess, not checked: the laptop panel is wired to the 610M, and the 4060's frames are copied
  to it.
- After the refresh − 3 cap, "cap off" FIFO read 0.10 to 18 ms frames, against a steady 5.6 to 6.6 ms
  at the start of the same run. With the cap off `Wait()` returns at once, so the loop is the same as
  at the start. The 240 Hz runs below found the cause: the NVIDIA driver's power saving.
- A present-mode switch in Release took one frame of about 100 ms (101.42 and 103.05 ms).
- Debug, caps 30 and 60 and a Ctrl+P: no `[WARNING]`, no `[ERROR]`, no asserts, 0 leaked blocks. Run
  again after the `vkQueueSubmit2` change (0890c7e): the same.
- `scripts\run-tests.bat`: all four configurations pass.
- The 240 Hz monitor, 2026-11-11, one Release run with step 6's line, leaving out the second each
  cap lands in:

  | setting | fps | frame min / max |
  |---|---|---|
  | FIFO, no cap, at the start | 239.6–239.8 | 3.40 / 4.89 ms |
  | FIFO, cap 100 | 100.0 | 9.97 / 10.02 ms |
  | FIFO, cap 119.88 (half) | 119.9 | 8.33 / 8.36 ms; one second 8.21 / 8.47 |
  | FIFO, cap 236.76 (refresh − 3) | 225.6–236.8 | 0.13 / 11.46 ms |
  | FIFO, no cap, after the caps | 231.5–239.6 | 0.11 / 12.79 ms |

  - Refresh − 3 doesn't hold on this monitor either: the frame rate drops to 225.6 and single frames
    reach 11 ms. On a VRR screen it would hold a steady 4.22 ms, so this screen acts as if VRR is off,
    at least in a window. A guess, not checked in the driver settings.
  - "No cap" after the caps reads 0.11 to 12.79 ms frames, against 3.40 to 4.89 ms at the start of the
    same run. The laptop panel did the same in step 5. With no cap `Wait()` returns at once, so the
    cause is outside the cap's code.
  - Cap 100's judder is for the eye, in step 7's scene.

- The uneven "no cap" frames, two one-minute runs on the 240 Hz monitor: 6 s with no cap, 8 s at cap
  30, then 40 s with no cap. `nvidia-smi --query-gpu=timestamp,clocks.gr,clocks.mem,pstate,
  utilization.gpu --format=csv -lms 500` logged the GPU during each run.

  | NVIDIA power management mode | GPU during the run | no cap, after the cap |
  |---|---|---|
  | default | P0 at startup, P3/P4, then P5 during the cap (255–570 MHz, memory 810 MHz), then bouncing P3 ↔ P5 | about half the seconds uneven: 224–234 fps, 0.7–8.4 ms frames |
  | Prefer maximum performance, for `bin\x64\Release\GameEngines5.exe` | P0 at 1,890 MHz, memory 8,101 MHz, 72 of 75 samples | all 41 seconds clean: 239.4–239.8 fps, 3.56–4.84 ms |

  - GPU use stayed at 20–45%. Scene0 is light work for the 4060, so the driver lowers the GPU's and the
    memory's clocks to save power, and keeps switching while the game runs. Each slow moment makes
    some frames miss their refresh. The engine's cap and timer are not involved.
  - The same effect likely widened the min/max of other runs in steps 5 and 6. Those weren't rerun.
  - Vakarian set the power mode back to the default in the NVIDIA Control Panel after these runs.
    build-and-test.md, "Running the game from a script", says when to switch it for a frame-time run.
