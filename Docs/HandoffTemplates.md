# DsonParser - Handoff Templates

The two fill-in forms the file-based handoff travels through (`Docs/AgentWorkflow.md`): the Director
fills the **task-file** when writing `.handoff/task-<id>.md`; the Implementer fills the
**feedback-file** when writing `.handoff/feedback-<id>.md`. Cold/on-demand. The `<id>` mint command
is in `Docs/Tooling.md` (UTC, run the clock - never typed from memory).

## Task-file template (Director -> Implementer)

The task-file must stand alone - the agent starts cold and may not be the default agent, so the
`Role:` line is mandatory (it is itself the role declaration the workflow expects).

```
Role: You are the **Implementer** for DsonParser - the role that edits source. Read
      AGENTS.md, Docs/Rulebook.md, and Docs/NativeBinding.md first, then make the change
      below. You may be any coding agent; these rules still apply.

Branch: `task/<id>` is checked out (off main). Do **not** run git - leave the tree dirty;
        the Director commits and squash-merges after verifying.

Mandate: <the user's this-session instruction ordering this task, quoted verbatim, with date. A
         missing or empty Mandate is a non-compliant handoff - halt and record it.>

Goal: <what the change should accomplish>

Context: <the files + facts the Implementer needs; point to Docs/dson-parsing-overview.md (map) +
         the source file(s) in scope; name the seam to extend (Docs/ReusePatterns.md) or state why
         a fresh path is warranted (COD-1)>

Task: <concrete, ordered steps or the precise change required>

Robustness & scope: <the complete solution for this Goal, not an MVP: the in-scope failure modes /
                    edge cases and how it handles them; the stability risk introduced (shared
                    helpers, ABI surface) and its containment; and anything deferred, with why that
                    is safe ("nothing deferred" is valid). Blank = non-compliant.>

Constraints: follow Docs/Rulebook.md (COD-1..9, CPP-1..2, COD-100/101) and Docs/NativeBinding.md
             (CPP-3/4) and self-audit after each edit. Do **not** edit docs - record any
             status/layout/routing consequence in the feedback Docs delta field; the Director owns
             the doc/version/pin sync at merge. The C ABI is published surface: any change to
             DsonParserAPI.h is breaking (COD-4) - flag it, and stage nothing version-side yourself.
             No silent fails - surface any blocker, gap, assumption, or duplication you could not
             cleanly avoid. Deliver the Robustness & scope bar; surface any shortfall.
             Environment hazards (any harness): Windows PowerShell 5.1; ASCII-only, BOM-less files
             (BOM-less UTF-8 non-ASCII decodes as cp1252 mojibake); targeted edits only, never
             wholesale file rewrites; C++14 only (v143 default) - no C++17+ constructs; UE-agnostic
             (no UE headers/types); close any running DsonTest2.exe / DsonLoadTest.exe before
             rebuilding (they pin DsonParser.dll - LNK1104).

Build & verify: per Docs/Tooling.md - resolve msbuild via vswhere, then
                  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
                  $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
                  & $msbuild DsonTest2.sln /p:Configuration=Release /p:Platform=x64
                (add /t:Rebuild to clear a stale pch; run the DsonTest2 harness from x64\Release\
                where useful). Iterate to a clean build before reporting.

Report: write your results to .handoff/feedback-<id>.md using the feedback template in
        Docs/HandoffTemplates.md. On any block - build failure, ambiguity, needed assumption, rule
        conflict - halt and report it there rather than guessing past it.

Feedback requested: <yes/no - if yes, what to assess before/instead of coding>
```

## Feedback-file template (Implementer -> Director)

```
Status: smooth | blocked

Agent: <your model + harness, best self-identification - unreliable, so the Director reconciles it
       against the user's launch-name, which is authoritative>

Files changed: <paths, one per line>

Build result: <exact command> -> <clean | warnings | errors, with the key lines>
Test result: <n/a - no automated suite (COD-8 dormant); harness PASS/FAIL lines if run>
Artifacts: <first lines (verbatim) of any NON-test produced artifact; "none" if none>

What I did: <concise account of the change>

Docs delta: <status/layout/routing/tooling changes this implies for Roadmap/Architecture/the entry
            guide - so the Director syncs the docs at merge; or "none">

Blockers & assumptions: <anything that blocked, any assumption made, any question for the Director -
                        or "none">

Notes: <optional: reasoning, alternatives considered, follow-ups>
```
