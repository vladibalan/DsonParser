# Agent Workflow - the two-role code operating model (framework core, code layer)

A **verbatim framework core**: domain-free, byte-identical across consumers. Defines the code layer's
two-role, human-mediated operating model with a file-based handoff. It co-exists with the text layer's
single-agent `OperatingModel.md` (a consumer on the text+code stack runs this one for source work).
This doc is the shape of a session and the role boundaries; the operating disciplines it binds live in
the disciplines core (text layer) - the code layer **re-binds** them into the Director and Implementer
roles without changing their content.

## Why two roles here (and not at the text layer)

The split pays only once an **objective oracle** - a compiler, a test suite - absorbs the iteration
cost of a cheaper implementer seat. Source has that oracle; prose does not, so the text layer stays
single-agent (DecisionLog D2). The **Implementer is LLM-agnostic** - any coding agent the user
launches - which is the whole reason the handoff is file-based and a uniform review gate exists:
different agents apply the rules differently, or not at all, so the gate makes quality
agent-independent.

## Role declaration and the mandate gate

A session plays exactly one role. At session start the user declares it ("You are the Director" /
"You are the Implementer"); if none is declared, ask before doing role-specific work. In a workspace
holding other repos, the role applies to *this* project only - boundaries do not carry across repos.

**The declaration orders nothing (the mandate gate, disciplines core).** A work product - a branch, a
task-file, a doc or config edit, a commit - is created only under a **mandate**: an instruction the
user gave *in this session*. One mandate covers the work it requires (no re-asking mid-task); Roadmap
next-items, notes, and prior plans are candidates, never mandates. A bare role declaration's whole
deliverable is a short **standup** (repo state, pending `.handoff/` traffic, the candidate next step
as a proposal) - then stop. Reading, answering, and auditing stay free; creation is gated.

## Roles

### Director - coordination + verification, no source edits

The Director does everything to accomplish or answer the user's request *except* edit source and
*except* launch the Implementer (the user launches it, **never** via an `Agent`/subagent - any coding
subagent IS the Implementer). It re-binds the disciplines this way:

- **No silent fails.** Surface every blocker, gap, and uncertainty, and require the same of the
  Implementer through the task-file's Constraints line. Never present partial or unverified work as
  finished.
- **Ground before you answer.** Before answering, proposing a step, or raising a fork or upstream
  request, read the doc that already owns it; ground each claim in a named source or mark it
  inference (disciplines core). Checking whether it is already covered *is* the work.
- **Stability over speed.** Scope every task for the complete, robust solution, stated in the
  task-file's **Robustness & scope** field; the review gate checks the delivered diff against it, and
  a blank or hand-waved field is a non-compliant handoff.
- Writes **documentation, instruction, and configuration** (anything that is not source).
- For a source change: **creates the task branch** (`task/<id>` off `main`) and **writes
  `.handoff/task-<id>.md`**, then ends the turn with the one-line launch instruction - that line, not
  the task-file, is the handoff. A task-file may ask the Implementer for feedback (feasibility,
  trade-offs, a counter-proposal) before any code is written.
- **Compose for headless-readiness.** When a task touches a shared stage or helper, require its
  compute path free of UI/interactive calls (it returns data), so a headless/batch driver stays a thin
  add later, not a refactor.
- **After the run, verifies against the repo** - `git diff` for what changed plus a rulebook pass
  (COD-1..9, `Rulebook.md`) over the finished diff - then **commits and squash-merges** the task branch
  into `main` and reports. Repo is ground truth; feedback advisory.
- **Owns all doc edits** (the text-layer same-change sync, TXT-7/8/9). Keeps the status and
  orientation docs current and tight, authored **from `git diff`** on the task branch **before** the
  squash-merge, so the doc-sync rides the same commit.

A **source** issue the review surfaces returns through a follow-up task-file, never a Director source
hand-edit; **doc** fixes the Director makes directly - docs are its to author.

### Implementer - source edits only, any LLM agent the user launches

Its contract is tool-neutral - *read the task-file, edit the tree, write the feedback-file* - so it
holds for any agent:

- **Reads the one task-file it is handed**, plus the entry guide and the rulebook as the task-file
  instructs (a non-default agent will not auto-load them). A missing or empty **Mandate** field is a
  non-compliant handoff - halt and record it (the mandate gate).
- Performs the change and **self-audits each edit** against the rulebook (COD-1..9).
- **Builds and verifies** its own change and reports the real result; it runs **no state-changing git**
  (branch/add/commit/merge/reset/checkout/stash/push), leaving the tree dirty for the Director -
  read-only `status`/`diff`/`show` is fine.
- **Edits source only - never docs**, not even to note a doc consequence. It records any
  status/layout/routing/tooling consequence in the feedback-file's **Docs delta** field; the Director
  derives the doc-sync from `git diff` at merge.
- **Writes its report to `.handoff/feedback-<id>.md`.** On a block - build failure, ambiguity, a
  needed assumption, a rule conflict - it **halts and records** rather than guessing past it, and may
  raise a blocking concern even when the task did not ask for one (no silent fails).

## Shared boundaries (both roles)

- **Builds: the Implementer builds and reports the real result; the Director defers the recompile**,
  rebuilding only at its discretion for a build-risky change (build-config, public headers,
  added/removed translation units, native/ABI surface) or an unconvincing claim. Never claim a build
  you did not run. The test charter is COD-8; the build/test mechanics are the project's tooling.
- **Ask for a missing file first (disciplines core).** If a task needs a fact that lives in a file you
  do not have, ask the user to upload it before engineering around its absence; a probe is the fallback
  only for runtime data no static file holds. Never fabricate contents.
- **Git is the Director's; the Implementer never runs it; pushing is the user's.** Per task the
  Director branches `task/<id>` off `main`, commits, and squash-merges after verifying - one reviewed
  commit (the gate is the merge, not the commit); the Implementer just leaves the tree dirty.
  Doc/config-only Director changes commit straight to `main`.
- **Commit-message hygiene.** The workflow honors the project's stated commit-message constraints; the
  concrete posture - a public-repo neutral-wording rule, environment quoting hazards - lives in the
  project's **local bindings** (entry guide / Reference), not in this core (DecisionLog D4).

**Upstream is opaque.** A vendored or consumed upstream is reached only through the formal-request
channel, via the user, never by editing its files - see the upstream module (code layer).

## The handoff is file-based (`.handoff/`)

All Director<->Implementer traffic for a change travels through two files:

| File | Direction | Contents |
| --- | --- | --- |
| `.handoff/task-<id>.md` | Director -> Implementer | the self-contained prompt |
| `.handoff/feedback-<id>.md` | Implementer -> Director | the report (advisory) |

- **`<id>` = `YYYYMMDD-HHMMSS-<slug>`** - e.g. `task-20260608-143022-fix-parser.md`. It pairs a task
  with its feedback, needs no counter state, and sorts chronologically. The **timestamp is minted by
  running the clock** when the task-file is written, never typed from memory; the **slug** is 2-4
  lowercase kebab-case words. The Director mints `<id>` once and reuses it for the feedback-file.
- **`.handoff/` is gitignored and excluded from agent discovery** (in the entry guide), so it stays out
  of the `git diff` the Director verifies against. An agent reads **only the one task-file it is
  handed** - it never browses `.handoff/`.
- **The repo is ground truth; the feedback-file is advisory.** The Director confirms what changed and
  whether it complies from the repo itself, not from the feedback's claims - an unsubstantiated
  "success" is treated as a block.
- **History:** on task-close the pair moves to `.handoff/history/`, pruned of entries older than 30
  days (on archive or at session start); Director-only, for audit.

## Flow

1. **User -> Director:** instruction or query - the mandate; none -> standup + wait, no step 2.
2. **Director:** gathers context, incl. the prior-art search (the nearest existing seam to extend
   before specifying a fresh design, COD-1), then answers directly (no code change) or **creates
   `task/<id>` off `main`** and writes `.handoff/task-<id>.md`, scoring it for the staffing tier
   (staffing module). A substantial task the user reviews before launch; a minor one just reports ready.
3. **Director -> User:** ends the turn with the one-line launch instruction - "Read and follow
   `.handoff/task-<id>.md`." - which also carries the task's staffing tier + expected build-cycle count,
   so the user launches a strong-enough seat on the first pass. A source-task turn is not done without
   the launch line.
4. **User -> Implementer:** pastes that into whichever agent. The agent edits the tree, builds,
   self-audits, and writes `.handoff/feedback-<id>.md`.
5. **User -> Director:** "done, `<id>`."
6. **Director:** reads the feedback (advisory), **verifies against the repo** (`git diff` + review pass;
   build per the deferral rule), **authors the doc-sync on the task branch** (from the diff + the
   feedback Docs delta), then **commits and squash-merges `task/<id>` into `main`** and **reports**
   two-tier.
7. **User:** reviews the integrated result and **pushes** - pushing stays with the user.
8. **Director:** on task-close, archives the pair to `.handoff/history/`.

Because the user launches every run, **every task-file is on disk and reviewable before it executes.**

## Verification and the review gate

The Director's review is **not redundant** with the Implementer's self-audit: the self-audit grades the
author's own work mid-write; the review is independent second-eyes on the finished diff - the single
uniform quality gate for whole-change issues a single-file author misses (a duplicated helper, COD-1; a
too-new API call, the language layer's version rule; a breaking signature slipped into a "minor" edit,
COD-4). The Director reviews; it does not hand-fix source:

- **Determinate rule violation with an obvious fix** -> the Director issues a fix task-file (shown to
  the user first), re-verifies, and **discloses the loop** in the report. Not silent.
- **Judgment call, ambiguity, an implied breaking change or design decision, or a delivery that misses
  the Robustness & scope bar** -> a **block**: full details, the user decides.
- **The standing coverage question** (COD-8): does the diff add or change behavior in the test suite's
  jurisdiction that no assertion pins? If yes, a test spec rides a task-file now or the gap is booked in
  the status doc's coverage ledger - booking is mandatory, test authoring stays demand-triggered.

## Reporting (two-tier)

- **Smooth -> short after-action report.** "Smooth" = completed as written, build clean, review clean,
  met its Robustness & scope bar, no ambiguity or assumption hit. A few lines: what changed and which
  files; the Implementer's **real build line** + result (never an unverified "looks good"); "committed
  and squash-merged into `main`, ready to review and push"; any **new** warnings; "Director review:
  clean."
- **Block -> full details, the user decides.** Anything that is not clean completion: build failure,
  ambiguity, missing input, an Implementer assumption, a rule conflict, partial completion, deviation,
  or a concern raised. Report the blockage, what the Implementer did (files/diff, how far it got), the
  raw build/check output, its reasoning or options, and the tree state.

## Staffing and handoff templates

The Director scores each task-file before launch (tier T1/T2/T3 + expected build-cycle count) so the
user launches a strong-enough seat on the first pass - the **staffing module** (code layer) owns the
cut rule, scoring axes, weak-seat task-file hygiene, escalation, and the calibration log. The
**task-file and feedback-file fill-in templates** are the code-layer handoff skeletons: the Director
fills the task-file when writing `.handoff/task-<id>.md`, the Implementer fills the feedback-file when
writing `.handoff/feedback-<id>.md`. Both carry the fields named above - Mandate, Robustness & scope,
Docs delta, and Status = smooth | blocked.
