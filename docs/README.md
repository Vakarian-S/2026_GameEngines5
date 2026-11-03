# Docs

**Last updated:** 2026-11-03.

A doc's folder says what kind of doc it is. Nothing inside a doc needs to say it again.

| open                | when you want                                                     |
| ------------------- | ----------------------------------------------------------------- |
| living/STATE.md     | what exists, and the architecture rules. Read it first            |
| living/ROADMAP.md   | the to-do list                                                    |
| living/PLAN.md      | the deep work happening now                                       |
| living/WORKFLOW.md  | how we work: the stages, the coding rules, the checks             |
| living/ECS.md       | the ECS design, its invariants and its house rules                |
| tools/              | how to use a tool, build the project, or run the tests            |
| dev-notes/README.md | why a finished feature is the way it is, and what it measured     |
| findings/           | the evidence behind a ROADMAP.md row                              |
| templates/          | the template a new PLAN.md starts from                            |
| archive/            | the plan that ended last, and the docs as they were then          |

The oldest snapshot sits outside the repo: `C:\Users\sebas\RiderProjects\2026_GameEngines5-docs-backup-2026-10-20`
holds every doc as it was before the 2026-10-20 reorganization.

**Kept up to date?**

- `living/`: yes. Out of date means wrong. Each starts with a "Last updated" line.
- `tools/`: yes, when the code a manual lists under `## Files` changes.
- `findings/`: no. The body stays as written. When every item in it is done or dropped, it is
  deleted.
- `dev-notes/`, `archive/`: no. Nobody updates them. They are deleted once settled (WORKFLOW.md,
  Archive).
- `templates/`: when the way we work changes.

How docs move between folders is in WORKFLOW.md, under "Archive".

Paths inside the docs are written from `docs/`. A bare name such as PLAN.md means living/PLAN.md.
