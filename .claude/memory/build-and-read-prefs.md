---
name: build-and-read-prefs
description: Workflow prefs — Implementer builds & verifies; Director re-runs the build in DsonParser but DEFERS it in the UE plugins (DsonToUnreal/DsonArtisan), rebuilding only for build-risky changes; confirm before reading >500-line files; step-by-step refactor approval
metadata: 
  node_type: memory
  type: feedback
  originSessionId: b6192ffc-0b77-45ac-af89-582dd7707524
---

**Workflow — file-based handoff (adopted 2026-06-08, this session).** The repo now
documents the full Director/Implementer model in `docs/agent-workflow.md` (reached
via CLAUDE.md) — defer to it for detail. Build-role essentials: the **Implementer**
(any LLM agent the user launches on a `.handoff/` task-file) builds and verifies its
own changes (`msbuild DsonTest2.sln /p:Configuration=Release /p:Platform=x64` + a
`DsonTest2` run where useful) and writes a feedback-file — never claim a clean build
it didn't run. The **Director re-runs that build itself to verify the returned
change** (repo = ground truth, feedback-file = advisory), runs a code-review pass,
and does NOT edit C/C++ source or launch the agent. The user launches each run and
handles git commits/pushes. (Until the 2026-06-08 change the Director *deferred*
builds — superseded **only for this msbuild parser repo**; the UE-host plugins later
diverged back to defer-and-review, rebuilding only for build-risky changes — see the
codification paragraph below.)

**Build-lock hygiene — `LNK1104: cannot open … DsonParser.dll` (learned 2026-06-08):**
two culprits hold the output DLL during this workflow, often together. (1) A lingering
`MSBuild.exe` *reuse* node left alive by a `/m` build — the Director verify build did
this; the node then contends with the user's Visual Studio build over the shared
`x64\Release\DsonParser.dll`. (2) A `DsonTest2.exe` left sitting at the harness's
`std::cin.get()` "Press Enter to exit…" prompt — it keeps `DsonParser.dll` **loaded
indefinitely**, and the Implementer is *told to run the harness*, so it can leave one
open (user's catch, 2026-06-08). Diagnose by checking BOTH `Get-Process DsonTest2`
and stray `MSBuild.exe` nodes; kill whichever (both safe — a test exe / reusable
workers that respawn). Prevent: Director runs verify builds with `-nodeReuse:false`
and in the foreground; redirect harness stdin (`< NUL`) so it can't park at the prompt.
The harness's unconditional pause is itself the foot-gun — making it conditional (skip
when args are passed or stdin is redirected) would end recurrence. Quick unblock:
Build → Clean + Rebuild.

**UBT "up to date" no-op on Director re-verify (DsonToUnreal UE-host build, learned
2026-06-11).** The plugin builds via `Build.bat DsonHostEditor` (not msbuild; see
[[compliance-run-definition]] + Docs/Tooling.md). Because the Implementer builds first
and leaves the tree dirty, the Director's verify build usually returns `Target is up to
date` (~0.4s no-op). Tooling.md counts that as success, but it only confirms artifacts
match source — it does NOT independently exercise the compiler. For a **build-risky**
change (parser-ABI X-macro rows, new UHT types, added/removed `.cpp`), force a real
recompile: touch the changed TUs (`(Get-Item $f).LastWriteTime = Get-Date`; touching the
widely-included `DsonParserFunctions.h` cascades a full-module rebuild), close the UE
Editor (the link step locks `UnrealEditor-DsonImporter.dll`), then build — so
`DsonParserAbiCheck.cpp` + UHT + the new code actually compile under your own invocation.
Confirmed clean on the Slice-5 ERC/JCM recipe change (18 actions, 0 warnings). **Current codification (DsonToUnreal `Docs/AgentWorkflow.md`):** the Director **defers** its own recompile for non-build-risky changes — verify via `git diff` + a CodeReviewRules pass, not its own build — and rebuilds only at discretion for a build-risky change or an unconvincing build claim. So the parser repo (msbuild) has the Director re-run the build; the UE-host plugins (DsonToUnreal **and DsonArtisan** — each has its own `Docs/AgentWorkflow.md` carrying the same defer-rule) have the Director defer-and-review, rebuilding only for build-risky changes. Followed across DsonToUnreal v3.5.0/v3.6.0 (2026-06-22) and confirmed on **DsonArtisan S2b (2026-06-24)**: the verify build no-op'd to `Target is up to date` (Implementer had built first), so for a build-risky change (new `.cpp` + `*.Build.cs` adding `InputCore`) I force-touched the 3 changed `.cpp` to recompile under my own invocation — 6 actions, 0 warnings.

**Build-ban history:** the former user-global `permissions.deny` on `Bash(msbuild:*)`
/ `MSBuild.exe` / `dotnet build` / `devenv` (added 2026-06-04) was removed from
`~/.claude/settings.json` on 2026-06-08, lifting the ban for ALL repos. Settings
can't tell a Director session from an Implementer one, so the role split is a
documentation convention, not harness-enforced. See [[dsontounreal-plugin-git-repo]]
— the plugin repo shares that user-global file, so its build rules may be out of sync.

Before reading any text file with more than 500 lines, confirm with the user first.

For multi-step refactors, the user approves each step individually. Present one step
at a time and proceed only after an explicit green-light; do not batch ahead.

**Why:** read/refactor prefs stated explicitly (2026-05-30, 2026-06-04); build ban
lifted 2026-06-08; the Director-as-verifier file-based handoff was designed and
adopted 2026-06-08 (this session).
**How to apply:** Implementer — build and report real results (even after small
edits) in the feedback-file. Director — write the task-file, hand the user a launch
one-liner, then verify the returned change with your own msbuild + code-review pass;
don't edit source or launch the agent. For files >500 lines, ask before Read. For
refactors, stop after each approved step and wait for the go-ahead.
