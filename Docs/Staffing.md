# Implementer Staffing - pre-launch seat tiering (framework core, code layer)

A **verbatim framework core**: domain-free, byte-identical across consumers. The Implementer is any LLM
agent the user launches (`AgentWorkflow.md`), so a too-weak seat surfaces only **after** a failed pass -
and a failed pass is expensive: two human-mediated round-trips, Director ground-truthing against the
repo, an escalation task-file, together dwarfing any per-seat price gap. It also inverts the token
economy (a weak seat thrashes more tokens than the strong seat would have spent, *then* adds the
round-trips). So the Director **scores every source task-file before launch** and hands the user a
**tier + expected build-cycle count** at the launch line (`AgentWorkflow.md` Flow step 3). Cold/on-demand.

## The tiers

| Tier | When | Seat |
| --- | --- | --- |
| **T1 mechanical** | Every deliverable matches a named in-repo precedent; no residual design decision; a failure would be a compile error. | Any agent. |
| **T2 guided** | Local design inside a pinned architecture; the seam to extend is named; existing tests stay green. | Mid-tier agent. |
| **T3 frontier** | A novel subsystem, a new test asserting new behavior, a cross-unit / cross-module invariant, or an unfamiliar platform surface. | Strongest available seat. |

**Two rules.** Tier = **MAX over phases, never the average** - spec-pinning shrinks the mechanical
phases, not the debug phase, so a task with one T3 phase is a T3 task. **Round up on doubt** - the
failed-pass cost is the tie-breaker.

## The six axes (each countable before launch)

| Axis | Pushes the tier up when |
| --- | --- |
| Residual "your call" decisions | open design decisions remain the Implementer's (the task-file Robustness & scope / Feedback-requested fields) |
| Precedent coverage | a deliverable has **no** named seam to extend (COD-1 / the reuse-seam index) - where the risk lives |
| **Debugging exposure (sharpest)** | a **new test asserts new behavior** -> a near-certain non-obvious first-run failure; "existing tests stay green" is only T2 |
| Cross-unit invariant coupling | the change is to a shared compute seam several call sites bind (COD-1 / COD-4) |
| Constraint checkability | the binding constraint is **judgment** (dedup, robustness) rather than a mechanical count (size, nesting) |
| Unfamiliar platform surface | a subsystem new to the codebase - it floors at T3 even fully spec'd (the trap you did not know to pin); higher layers sharpen this axis for their platform/engine |

A public-signature (breaking) change (COD-4) and a version-carrier obligation (COD-11) both read as
cross-unit coupling.

## Slicing for tier - cut at arc-planning time

Tiering scores the task you were handed; slicing shapes the task you write. Cut arc boundaries so the
mechanical residue crystallizes into its own independently landable, suite-gated tasks, then staff each
honestly - no new machinery, the self-contained task-file is already the weak-seat contract:

- **Slice only at task boundaries** - each slice builds, passes the suite, and reviews standalone on its
  own `task/<id>` branch.
- **Never split one change across seats; never relay a working tree between seats** - a half-change
  cannot self-verify, and the stronger seat inherits the weaker one's latent errors. (The review-gate
  fix loop is not a relay: the Director ground-truths the tree and names the defect between runs.)
- **Extraction patterns:** pinned-literal regression tests after a landed feature (known values = T1, a
  test that must *discover* its values = frontier); behavior-freeze refactor batches, suite-gated; fixes
  where the Director names the root cause and the site; pilot-then-batch (a strong seat lands the
  precedent, cheap seats replicate it as separate tasks).
- **Never slice:** a novel feature's debug tail (MAX-over-phases already prices it); a cross-unit
  invariant (a cut multiplies interface risk); work whose verification exists only after the whole arc
  integrates.

## Weak-seat task-file hygiene (each item is a measured failure mode)

At a low tier the task-file carries the whole burden - a foreign harness loads none of this machine's
config, so the task-file is the only carrier that reaches every seat:

- **Pin full literals** - expected values, counts, exact paths; "derive it" is a tier raiser.
- **Every step an explicit action, no implied steps** - weak seats run out of list where strong seats
  infer.
- **Inline the environment hazards verbatim** - shell-quoting traps, file encoding + line-ending
  discipline (the local bindings name the concrete ones).
- **Self-checks with the expected output stated** - the seat compares, never interprets.
- **Freeze, don't recover** - on any unexpected result: stop, run nothing further, report the verbatim
  output. Recovery attempts, not failures, are where weak seats do damage.
- **Append/insert-only edits where possible; check the diff shape** before the review pass - wholesale
  rewrites are where encoding corruption ships.
- **Harness-floor probe** - before first real work on a new model x harness combo: read one named file,
  quote one named line, write one scratch file. Below floor disqualifies at any tier.

## Token economics

Forecast = **fixed doc ingest** + **edits** (scale with step count) + **iteration multiplier** (build /
test cycles: T1 1-2, T2/T3 3-8, an under-staffed seat unbounded - this term dominates, and it is the one
mis-staffing blows up). **The delegation bar:** delegation pays only when the capable-seat tokens
avoided exceed the spec + review tokens added, and the task-file pin must be a **byproduct of design
already done**. If new Director work is needed specifically to make a slice weak-safe, the authoring cost
eats the savings - never manufacture pins.

## Escalation discipline

A debugging block is **never re-handed at the same tier.** The Director diagnoses from the repo first
(the repo is ground truth; an unsubstantiated success is a block, `AgentWorkflow.md`); the escalation
task-file then names the **root cause** or the **exact observation step** to run - never "try again".
Two failure signatures, stable per model:

- **optimistic-stub** - a placeholder reported as done (a silent fail; repo-ground-truth verification
  exists to catch it). A seat that does this earns suite-gated tasks afterward.
- **honest-thrash** - N variations of one wrong theory while the produced artifact goes unread (the
  feedback **Artifacts** field forces that first read).

## The calibration loop

The feedback-file carries two staffing fields - **Agent** (the seat the user launched; user-supplied is
authoritative, a model's self-report advisory) and **Artifacts** (the first lines of any runtime
artifact, pasted verbatim - makes observation mandatory and self-reports checkable). The Director keeps a
**calibration log**: a row appended **at authoring** (predicted tier + forecast cycles), completed **at
close** (agent, actual cycles from the feedback build digest, outcome). A handful of rows turns tiering
from intuited into calibrated. The project owns the log (a local doc); its empty shape:

| Date | Task `<id>` | Predicted tier | Forecast cycles | Agent | Actual cycles | Outcome |
| --- | --- | --- | --- | --- | --- | --- |
