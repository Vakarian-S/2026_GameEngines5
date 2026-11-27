# Workflow

**Last updated:** 2026-11-26.

How work on this engine runs: the stages a feature goes through, the plan file, and the rules for
code, design and checks.

## Contents

- [The stages](#the-stages)
- [Each stage](#each-stage)
- [Status words](#status-words)
- [The plan file](#the-plan-file)
- [Coding rules](#coding-rules)
- [Design judgement](#design-judgement)
- [Checks](#checks)

## The stages

| # | stage | what happens | where it lives |
|---|---|---|---|
| 1 | outline | an idea and its reason, one row | ROADMAP.md |
| 2 | prototype | optional: a throwaway build to test feel, before design commits to anything | its own branch |
| 3 | design | the problem, a table of epics, then each epic's tasks and decisions | PLAN.md |
| 4 | build | the code, with its tests | the code |
| 5 | commit | the diff is checked against the docs | git |
| 6 | archive | each epic's section becomes a dev-note, and STATE.md is updated | `dev-notes/` |

## Each stage

**Outline.** A row in ROADMAP.md: what it builds and why, written as `templates/ROADMAP.md` says.
Nothing is designed yet. When a row starts, it leaves ROADMAP.md: its notes move to PLAN.md, and
anything the initiative will not do becomes its own row.

**Prototype.** Only when a decision could be built and then dropped, such as how a feature feels. It
lives on its own branch, in its own worktree, so the main build stays clean. What it teaches goes
into `findings.md` at the root of that branch, and the design starts from it.

**Design.** The problem first, then the epics, then each epic's tasks. Decisions are written only
for tasks that hold a real choice. [The plan file](#the-plan-file) has the layout.

**Build.** The code comes with its tests.

**Commit.** Compare the changed files with the `## Files` section of each doc in `tools/`. Each doc
that covers a changed file gets checked for an update. A `## Files` line that names a class instead
of a path matches by meaning.

**Archive.** When every epic of the initiative is done, each epic's section becomes a dev-note, one
per feature. STATE.md is updated, and PLAN.md starts again from `templates/PLAN.md`.

- A new dev-note starts with a "> **Legacy doc.**" line under its title. Dev-notes are never edited
  afterwards, not even their links. Only `dev-notes/README.md`, the index, changes.
- A dev-note has no open items. Each open item becomes a ROADMAP.md row, a line in a living doc, or
  is dropped.

**Hygiene.** A record stays only until what it records is settled. After an initiative is done,
check which dev-notes and archives are settled, and delete them. Name each deleted file in the
commit message. Run `git ls-files <path>` before deleting: an untracked file has no copy in git.

## Status words

- **ROADMAP.md rows** have no status: every row there is to-do.
- **PLAN.md epics:** what the epic is ready for, written only when PLAN.md is edited: outline when
  the initiative is planned, designed when the design talk ends, or done with the commit hash.

## The plan file

PLAN.md holds one **initiative**: one goal, reached through several epics.

- **Epic.** A numbered piece of the initiative, E1, E2, with a "done when" line. Each epic ends in a
  commit.
- **Task.** A numbered piece of an epic: E4.1 is task 1 of epic 4. A task says what to do.
- **Decision.** A note under a task, only when the task holds a real choice: an option we reject, and
  picking wrong costs rework in other code. It reads "chose X over Y, because Z". A task with no real
  choice gets no decision.

The top of PLAN.md is the glance: the initiative in a few sentences, the epics table, and the stage
legend. Each epic's section then has:

1. **The problem**, in a few sentences.
2. **A task table**: each task's number and title, one line each.
3. **One heading per task** (`### E4.1. Title`), with what to do and its decisions under it.
4. **What was built**: the files and type names where each task landed, and each change from the
   task, with its reason.
5. **What was checked**: the tests added and the runs, with their results.

An epic that came from a prototype names the prototype's branch.

**What to keep.** Keep a line only when the code and the git log cannot tell a reader the same thing:

- why a choice was made, when the reason is not obvious;
- a rejected option that someone would likely propose again, with the reason it lost. A rejected
  option with no reason gets cut;
- a measured number, with the command that produced it.

Cut what a function does, bugs already fixed, and history told as a story. Keep a fixed bug only when
it explains a rule that looks strange, such as `Protobuf_USE_STATIC_LIBS` in the networking plan's
E3.3.

## Coding rules

**Comments**

1. Never use `//`. Use `/** */` above declarations, so Rider shows them on hover, and `/* */` above a
   line inside a function body.
2. Section banners are `/* ===== Section ===== */`.
3. No trailing comments at the end of a line. The one exception is `// NOLINT(check-name)`, because
   Rider only honours it in that form.
4. One or two lines per comment. A comment does not teach, justify a design or repeat the code. It
   explains a *why* only when the why is not obvious, in one sentence.
5. Each `/** */` makes sense on its own, because Rider's tooltip shows only that comment. Name the
   subject every time; never write "the same as above".
6. Plain, everyday words: fastest, slowest, adds, biggest.

**Names**

7. Name a class or function for what it is, not for what it is used for: `TemporaryCommandBuffers`,
   not `OneTimeCommands`.
8. One job per class.
9. An ECS name ends in what kind of thing it is, as Unreal Mass names its fragments and tags. A
   system ends in `System` (`ShootSystem`), a component with data in `Component`
   (`HealthComponent`), and an empty component in `Tag` (`BossTag`). The name in front still
   follows rule 7. The suffix lets a search for one kind find all of it.

**Constants**

10. A constant goes in its `.cpp`'s anonymous namespace, and in the class only when header code or
    another file reads it. Changing a header constant rebuilds every file that includes the header.

**Hot loops**

11. `[[unlikely]]` goes only on a rare branch whose body is an out-of-line call (an error, a log, a
    debug hook) inside a per-chunk or per-row loop, and `EcsBench` confirms it. On `ForEachChunk`'s
    breakpoint check it won back a 45% slowdown; on `Archetype::AddRow`'s new-chunk branch it gained
    nothing beyond noise.

## Design judgement

1. **Engine value.** Judge a feature by what it buys in a shipped game, what it costs, and whether this
   engine's scale shows any of it. Say plainly when a feature gives control but no speed at this scale.
2. **No caller, no code.** Name the code that will read a struct, field or function before designing
   it.
3. **Measure before claiming.** Any number with a unit comes with the command that produced it.
   Anything else is labelled a guess.
4. **Sources.** When a call leans on what another engine does, check its primary docs first, or say
   it is from memory.

## Checks

- **A task is checked** by building, running, closing the window and reading the error stream: no
  `[WARNING]`, no `[ERROR]`, and 0 leaked blocks. `scripts\run-tests.bat` passes in all four
  configurations.
- **The include rules are checked** with `scripts\check-includes.ps1`, which exits 0 when every
  `#include` follows STATE.md rule 2. tools/build-and-test.md, "Checking the include rules", has
  the details.
- **The game is checked by running it** with `scripts\run-game.ps1`, which starts it, presses keys,
  clicks, takes screenshots, closes it and prints those checks. tools/build-and-test.md, "Running
  the game from a script", has the steps.
- **Timings** move about 8% from run to run, and the first run of a fresh build is about 25% slower.
  Compare the mean of three runs, and drop the cold one. Counters (migrations, bytes, allocations)
  are exact on one run.
