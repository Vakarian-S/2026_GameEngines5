# Workflow

**Last updated:** 2026-11-03.

How Vakarian and Claude work on this engine: the stages a feature goes through, who does what in each,
and the rules for code and design. Every Claude session reads this before design or build work.

## Contents

- [The stages](#the-stages)
- [Each stage](#each-stage)
- [Status words](#status-words)
- [Coding rules](#coding-rules)
- [Design judgement](#design-judgement)
- [Checks](#checks)

## The stages

| # | stage | what happens | where it lives |
|---|---|---|---|
| 1 | outline | an idea and its reason, one row | ROADMAP.md |
| 2 | prototype | optional: a throwaway build to test feel, before design commits to anything | a branch, and `findings/` |
| 3 | design | the problem, a table of steps, then one decision at a time | PLAN.md |
| 4 | build | Vakarian writes the code; Claude gives hints, writes the tests and reviews | the code |
| 5 | recap | Vakarian explains the work back; Claude grades each claim | chat |
| 6 | commit | Claude checks the diff against the docs; Vakarian writes the message and commits | git |
| 7 | archive | the plan's sections become dev-notes, and STATE.md is updated | `dev-notes/` |

## Each stage

**Outline.** A row in ROADMAP.md: what it builds and why. Nothing is designed yet. When a block becomes
the next plan, it moves to PLAN.md.

**Prototype.** Only when a decision could be built and then dropped, such as how a feature feels.
Claude builds it fast on its own branch, and Vakarian runs only the exe, so his own build stays his.
What he finds goes into `findings/`, and the design starts from it. Claude stops building at 45% of
the weekly usage or 60% of the 5-hour usage.

**Design.** A conversation, not a spec.

- Start with the problem, then a table of the steps.
- One decision per message, with one short reason. Go deeper only when Vakarian asks.
- Steps are numbered and decisions are lettered: decision a, decision b. A reference names both, as
  "step 3, decision b", so a decision never reads as a step.
- When a call leans on what another engine does, check its primary docs first, or say it is from
  memory. Say which earlier session answered the same question, and which question it answered.
- In idea talks, never steer back to the step. Vakarian says when the talk ends.
- Questions go in plain prose.

**Build.**

- Hints first. Vakarian writes the code. Claude gives code blocks only when he asks.
- Claude never edits Vakarian's files, docs included, unless he asks in that message.
- Claude writes the tests.
- "How's it looking" or "check it" means Claude reads the code, builds it, runs the tests and the
  scenes, and reports what it found.

**Recap.** Vakarian explains the work back in his own words. Claude grades each claim correct, close or
wrong, and says why for each one that is not correct.

**Commit.** When Vakarian mentions a commit, Claude compares the changed files with the `## Files`
section of each doc in `tools/`, names each doc that covers a changed file, and asks whether it needs
an update. Vakarian writes the commit message.

**Archive.** When every step of the plan is done, each step's section becomes a dev-note, one per
feature. STATE.md is updated, and PLAN.md starts again from `templates/PLAN.md`.

**Hygiene.** A record stays only until what it records is settled. Git keeps every deleted file,
so deleting loses nothing. After a plan is done, check which dev-notes, findings and archives are
settled, and delete them. Name each deleted file in the commit message.

## Status words

- **ROADMAP.md rows:** to-do, or in progress once the block has moved to PLAN.md. A finished row leaves
  the file.
- **PLAN.md steps:** what the step is ready for, written only when PLAN.md is edited: outline when
  the plan is made, designed when the design talk ends, or done with the commit hash. Prototype,
  build and recap happen in the code and the chat, so they get no status word.

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

## Design judgement

1. **Engine value.** Judge a feature by what it buys in a shipped game, what it costs, and whether this
   engine's scale shows any of it. Say plainly when a feature gives control but no speed at this scale.
2. **No caller, no code.** Name the code that will read a struct, field or function before designing
   it.
3. **Measure before claiming.** Any number with a unit comes with the command that produced it.
   Anything else is labelled a guess.

## Checks

- **A step is checked** by building, running, closing the window and reading the error stream: no
  `[WARNING]`, no `[ERROR]`, and 0 leaked blocks. `scripts\run-tests.bat` passes in all four
  configurations.
- **The game is checked by running it,** not by asking Vakarian: `scripts\run-game.ps1` starts it,
  presses keys, clicks, takes screenshots, closes it and prints those checks.
  tools/build-and-test.md, "Running the game from a script", has the steps.
- **Timings** move about 8% from run to run, and the first run of a fresh build is about 25% slower.
  Compare the mean of three runs, and drop the cold one. Counters (migrations, bytes, allocations)
  are exact on one run.
