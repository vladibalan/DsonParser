---
name: no-asset-specific-test-oracles
description: "Automated/UE-Automation testing is NOT an approved architecture feature — don't add it in task-files; verify via the user's editor bake + tagged diagnostic logs (Director reads DsonArtisanHost.log). Any check asserts general invariants, never an asset-specific captured oracle."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 411d5a1f-5d4a-4c7b-a3c9-9f89bca97b00
---

Two linked rules on testing/verification in the DsonArtisan/DsonToUnreal repos, both surfaced by the rigid-follow Slice-2 work:

**1. Automated testing is not an approved architecture feature.** I introduced UE Automation tests (`WITH_DEV_AUTOMATION_TESTS`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST`) in S2a and propagated them through the S2b/S2c task-files **without the user adopting automated testing**. The user isn't against it but wants it to be a **deliberate, separately-developed feature — not a conjecture/afterthought** smuggled in via task-files. **How to apply:** don't add automation/unit tests by default; if automated testing is wanted, propose it as its own deliberate design. (One S2a test, `DsonArtisan.CpuDqs`, already shipped unapproved — pending that decision.)

**2. Verify via the user's editor bake + diagnostic logs.** The approved channel: the USER re-imports/bakes/places the asset in the Editor; the code emits **tagged one-shot diagnostics** (`[rigid-follow-diag]` etc.); the Director reads `DsonArtisanHost.log` for ground-truth. This exercises the REAL pipeline (capture → resolution → eval), which a synthetic unit test never touches — strictly better per [[verify-the-actual-failure-mode]]. Precedent: Slice-1's own `[rigid-follow-diag]` logging (added for measurement, removed after confirmation). Log the **actual** computed values; **never a hardcoded expected constant captured from one asset** (the Importer once stranded such an "oracle matrix" in shipped code) and never branch production code on a specific asset. Litmus: would the check make sense with zero real assets imported?

**3. Diagnostics must be self-referential — don't ask the user which asset.** (2026-07-06, geograft Slice-4 planning) I asked the user to name the figure + geograft "to write the verification against a real target"; pushback: the generic logs already carry that. Self-describing diagnostics + **relative** pass criteria (internal-consistency counts like `scattered==interior`; `transferred>0` shaped vs `~0` base) mean the Director never needs the asset identity to read a bake. When a measured value needs a reference (does interior displacement track the shape?), **log the reference magnitude beside the measured one** (`baseMax/baseMean` next to `interiorMax/interiorMean`) so one line is self-contained — never hold the reference in your head or ask for the asset. **How to apply:** state the asset *property* the test needs (e.g. "a strongly-shaped G9"), not an identity; add controls (base-figure, no-graft) to disambiguate weak-signal from no-fire; make the log answer the question on its own.

Related: [[director-runtime-verify-via-user-import]], [[dsonartisan-host-runtime-log]], [[verify-the-actual-failure-mode]], [[gate-process-changes-on-recurrence]], [[value-types-are-scaffolding]].
