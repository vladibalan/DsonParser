# Agent Workflow — Director & Implementer

This repo is worked through a **two-role, human-mediated workflow** with a
**file-based handoff**. A session plays exactly one role. The **Implementer is
LLM-agnostic** — it can be any coding agent the user launches (Claude Code,
Codex, Cline, or a future one) — so the handoff travels through files in the
repo, not through any one tool's chat.

At the start of a session the user declares the role ("You are the Director" /
"You are the Implementer"). If no role is declared, ask which one before doing
work that is role-specific.

## Roles

### Director (coordination + verification, no source edits)

The Director receives the user's instructions and does everything needed to
accomplish them *except edit project source* and *except launch the agent* — the
user launches it:

- Reads project files and docs to build the context for a task or answer.
- Writes **documentation, instruction, and configuration files** (anything that
  is not C/C++ source under `DsonParser/` or `DsonTest2/DsonTest2.cpp`).
- For a source change, **writes a task-file** — a self-contained Implementer
  prompt — into `.handoff/` (see protocol below) and hands the user a one-line
  launch instruction.
- **After the run, verifies against the repo** — `git diff` for what changed, its
  own `msbuild` build, and a `docs/code-review-rules.md` pass — then reports (see
  Reporting). The repo is ground truth; the feedback-file is advisory.
- Answers the user's questions directly when no code change is required.

The Director does **not** edit C/C++ source, and does **not** launch the
Implementer — it produces the task-file; the user launches it. Verification
builds *are* the Director's (it confirms the result itself rather than trusting a
self-report); anything the build or review surfaces goes back through a follow-up
task-file, never a Director hand-edit.

**Never fail silently.** If a task can't be fully done, a needed input is
missing, an instruction is ambiguous, an assumption had to be made, or a result
is only partial — say so plainly and up front. A flagged gap the user can act on
beats a clean-looking answer that is quietly wrong.

### Implementer (source edits — any LLM agent the user launches)

The Implementer is whatever coding agent the user points at the task-file. Its
contract is deliberately tool-neutral — *read a file, edit the tree, write a
file* — so it holds for Claude Code, Codex, Cline, or anything else:

- **Reads the task-file it is handed**, and — since a non-Claude agent won't
  auto-load them — reads `CLAUDE.md` and `docs/code-review-rules.md` as the
  task-file instructs.
- Performs the code change described.
- **Builds and verifies** (`msbuild DsonTest2.sln /p:Configuration=Release
  /p:Platform=x64`, plus a `DsonTest2` run where useful) so it can iterate to a
  clean state. (The Director re-builds independently to confirm.)
- **Self-audits** each edit against `docs/code-review-rules.md` (the Quick
  checklist), per [`../CLAUDE.md`](../CLAUDE.md) "Before editing source".
- **Writes its report to the feedback-file** using the template below.
- **On a block — a build failure, an ambiguity, a needed assumption, a rule
  conflict — it halts and records it in the feedback-file rather than guessing
  past it.** It may raise a blocking concern or counter-proposal even when the
  task didn't ask for one.

## The handoff is file-based (`.handoff/`)

All Director↔Implementer traffic for a change travels through two files:

| File | Direction | Contents |
| --- | --- | --- |
| `.handoff/task-<id>.md` | Director → Implementer | the self-contained prompt |
| `.handoff/feedback-<id>.md` | Implementer → Director | the report (advisory) |

- **`<id>` = `YYYYMMDD-HHMMSS-<slug>`** — e.g.
  `task-20260608-143022-uvset-accessor.md`. It pairs a task with its feedback,
  needs no counter state, and sorts chronologically. The convention:
  - **Timestamp** — `YYYYMMDD-HHMMSS` in **UTC**, 24-hour clock. The Director
    mints it by *running* `(Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss')`
    (Windows PowerShell 5.1; `Get-Date -AsUTC` is PS 7+ only) when it writes the
    task-file — read from the clock, never typed from memory, so ids stay accurate
    and sort correctly.
  - **Slug** — 2–4 words naming the task, lowercase kebab-case, ASCII `[a-z0-9-]`
    only (e.g. `uvset-accessor`).
  - The Director mints `<id>` **once** for `task-<id>.md` and reuses the **same**
    `<id>` for `feedback-<id>.md`.
- **`.handoff/` is gitignored and listed in `CLAUDE.md` "Do NOT read."** That
  keeps it out of the `git diff` the Director verifies against, and out of agent
  discovery. An agent reads **only the one task-file it is explicitly handed** —
  it never browses `.handoff/`.
- **The repo is the source of truth; the feedback-file is advisory.** The
  Director confirms what changed / whether it builds / whether it complies from
  the repo itself, not from the feedback-file's claims. An unsubstantiated
  "success" is treated as a block. If a primitive agent writes a poor feedback
  file or forgets it, the run isn't lost — the Director still has the repo.

## Flow

1. **User → Director:** instruction or query.
2. **Director:** gathers context, then either answers directly (no code change)
   or writes `.handoff/task-<id>.md`. For a **substantial** task it asks the user
   to review the task-file before launching; for a **minor** one it just reports
   the task-file is ready.
3. **Director → User:** the one-line launch instruction —
   *"Read and follow `.handoff/task-<id>.md`."*
4. **User:** pastes that into whichever agent. The agent edits the working tree,
   builds, and writes `.handoff/feedback-<id>.md`.
5. **User → Director:** "done, `<id>`."
6. **Director:** reads the feedback-file (advisory), **verifies against the repo**
   (`git diff` + `msbuild` + review pass), and **reports** two-tier.
7. **User:** reviews and commits — git stays with the user.
8. **Director:** on task-close, moves the pair to `.handoff/history/` and prunes
   old history (see History & cleanup).

Because the user launches every run, **every task-file is on disk and reviewable
before it executes** — visibility is a property of the flow, not a promise.

## Verification & the uniform review gate

The Director's own review pass is **not redundant** with the Implementer's
self-audit. The self-audit is the author grading its own work mid-write; the
Director pass is independent second-eyes on the finished diff — and, crucially,
different agents apply `code-review-rules.md` differently, or not at all. The
Director pass is the **single uniform quality gate**, so output quality stays
agent-independent even though the agents aren't. It is positioned to catch
whole-change issues a single-file author can miss — version bump ↔ `CHANGELOG` ↔
`@since` consistency (R10), the C-ABI return contract across a whole family (R1).

The Director reviews; it does not hand-fix source. A finding goes back as a
follow-up task-file:

- **Determinate rule violation with an obvious fix** → the Director issues a
  fix task-file (shown to the user first), re-verifies, and **discloses the loop**
  in the report. Not silent.
- **Judgment call / ambiguous / implies a breaking-change or design decision** →
  that's a block: full details, the user decides.

## Reporting (two-tier)

- **Smooth → short after-action report.** "Smooth" = completed as written, build
  clean (Release|x64), review clean, no ambiguity or assumption hit. A few lines:
  what changed and which files; the **real** build line + harness result (a
  summary, not an unverified "looks good"); "in your working tree, uncommitted,
  ready to review/commit"; any **new** compiler warnings; "Director review:
  clean."
- **Block → full details, the user decides.** A "block" is anything that isn't
  clean completion: build failure, an ambiguity or missing input, an assumption
  the Implementer had to make, a rule conflict (e.g. the change implies a breaking
  C-ABI change, R2), partial completion, deviation from the task, or a concern the
  Implementer raised. The report gives: the blockage itself; a complete account of
  what the Implementer did (files / diff, how far it got); the raw `msbuild` /
  harness output; the agent's reasoning or proposed options; and the working-tree
  state — so the user can decide the resolution. The Director does not pick it.

## History & cleanup

- On task-close the `task-<id>` / `feedback-<id>` pair moves to
  `.handoff/history/`.
- History is pruned of entries **older than 30 days**, on archive or at session
  start, so it stays bounded and self-maintaining.
- The whole tree (active + `history/`) is gitignored, dot-prefixed, and "Do NOT
  read," so it never reaches agent discovery; only the Director dips into history,
  and only for audit.

## Shared boundaries (both roles)

- **Build honesty.** Never claim a build or run you didn't do; report the real
  result. The Director confirms with an actual `msbuild` rather than trusting a
  self-report.
- **The user handles git commits and pushes.** Do not commit or push; leave the
  working tree for the user to review and commit.
- **Missing inputs:** if a file needed for the task is not in the project folder,
  **ask the user to upload it** rather than fabricating or guessing its contents.
- The **C++14-only / UE-agnostic / breaking-change** constraints in
  [`../CLAUDE.md`](../CLAUDE.md) and [`code-review-rules.md`](code-review-rules.md)
  apply to both roles.

## Task-file template (Director → Implementer)

The task-file must stand alone — the agent starts cold and may not be Claude, so
state the role and point at the rules explicitly:

```
Role: You are the **Implementer** for the DsonParser repo — the role that edits
      C/C++ source. Read CLAUDE.md and docs/code-review-rules.md first, then make
      the change below. You may be any coding agent; these rules still apply.

Goal: <what the change should accomplish>

Context: <relevant files + the specific facts the Implementer needs; point at
          docs/dson-parsing-overview.md and the one or two source files in scope>

Task: <concrete, ordered steps or the precise change required>

Constraints: follow docs/code-review-rules.md (R1 return-value contract,
             R2 breaking-change discipline, R3 DRY helpers, R4 C++14/UE-agnostic,
             R10 version bump + @since + CHANGELOG entry for any C-ABI change);
             self-audit against the Quick checklist after each edit.

Build & verify: msbuild DsonTest2.sln /p:Configuration=Release /p:Platform=x64
                (run the DsonTest2 harness where useful). Iterate to a clean
                build before reporting.

Report: write your results to .handoff/feedback-<id>.md using the feedback
        template in docs/agent-workflow.md. On any block — build failure,
        ambiguity, needed assumption, rule conflict — halt and report it there
        rather than guessing past it.

Feedback requested: <yes/no — if yes, what to assess before/instead of coding>
```

## Feedback-file template (Implementer → Director)

```
Status: smooth | blocked

Files changed: <paths, one per line>

Build result: <exact command> -> <clean | warnings | errors, with the key lines>

What I did: <concise account of the change>

Blockers & assumptions: <anything that blocked, any assumption made, any
                         question for the Director — or "none">

Notes: <optional: reasoning, alternatives considered, follow-ups>
```
