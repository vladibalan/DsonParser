---
name: editor-checks-are-users-job
description: User personally performs every Editor/GUI verification step across the Director/Implementer workflow repos; Director must never write a task-file asking the Implementer to do one
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 40461e26-972e-40bc-9b16-6befb401ac14
---

The user personally performs every Editor/GUI verification step across this
Director/Implementer workflow — Genesis import runs, menu-driven checks, anything
requiring the live Unreal Editor UI. **The Director must never write a task-file that
asks the Implementer to do one** (e.g. "verify via the dev-menu enumerate," "run an
import and check the result").

**Why:** user correction (2026-07-11), on `task-20260711-140417-scene-manifest-impl.md`
(DsonToUnreal): "I instructed the Director to not ask implementers to do editor checks. I
do that." This generalizes [[director-runtime-verify-via-user-import]] (the Director's
*own* inability to run imports headlessly) to the task-file itself: even on rounds where
an Implementer agent could physically launch the Editor, that verification step still
isn't the Implementer's to own — it stays with the user, on the user's own timeline,
regardless of which agent holds the Implementer role that round.

**How to apply:** when writing a task-file as Director, scope "Build & verify" to what
the Implementer can actually do standalone — compile clean, self-audit against
`CodeReviewRules.md`, static review of the diff. Do not add an editor-driven acceptance
check (dev-menu action, import run, visual compare) as part of the Implementer's
deliverable or as a condition for reporting "smooth." Report the code + clean build as
done; the user runs their own check whenever convenient and reports back separately. The
task that surfaced this had asked the Implementer to verify a new scene-enumerate feature
against a worked `.duf` fixture via a dev-menu action before reporting done — the user
corrected this mid-session and confirmed the clean build alone was sufficient to close
the round. Now codified at the doc level too: DsonToUnreal `Docs/AgentWorkflow.md` (Shared
boundaries + the task-file template's Build & verify line, commit `4b25439`).
