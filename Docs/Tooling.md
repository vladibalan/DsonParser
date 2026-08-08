# DsonParser Tooling

Build/git tooling. The **git branch-per-task workflow** below is the code layer's; the **build &
verify** mechanics are the cpp layer's, merged here at materialization. The git policy + role split
- who runs git, push ownership - live in `Docs/AgentWorkflow.md`.

## Build & verify

The build unit is the solution, driven by **msbuild**, which is **not on `PATH`** in a plain shell -
resolve it once via `vswhere` (fixed install location, edition-agnostic), then build:

```
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
& $msbuild DsonTest2.sln /p:Configuration=Release /p:Platform=x64
```

- A clean build exits 0 with **no new warnings** (CPP-2: warnings are findings). An "up-to-date"
  result counts as success, but an incremental build can report success without recompiling - use a
  **full rebuild** (`/t:Rebuild`) as the real from-scratch check before a handoff closes.
- Gotchas that bite THIS project: a stale `pch` from a different toolset version trips incremental
  builds (`/t:Rebuild` clears it); a still-running `DsonTest2.exe` / `DsonLoadTest.exe` holds
  `DsonParser.dll` open and breaks a rebuild with `LNK1104` - close test exes first (`DsonTest2`
  waits on a keypress at exit, so it is easy to leave running).
- Test exes build to `x64\Release\` beside `DsonParser.dll`; run them from there so `DsonLoadTest`
  resolves the DLL. Piped-EOF harness exit is 255, not a failure - read the PASS/FAIL lines
  (`Docs/Reference.md`).

## Git workflow (branch-per-task)

Branch/commit/merge mechanics for the **Director**; `<id>` is the handoff id, branch `task/<id>`.

- **Open** off `main`: `git switch -c task/<id> main`.
- **Base / nesting.** A minor task spawned mid-task branches off the in-progress parent **only if**
  it needs that parent's unmerged changes, else off `main`; children merge up. The task-file's
  `Branch:` line records it.
- **Integrate** once the Director has verified (`git diff` + review pass): `git switch main` ->
  `git merge --squash task/<id>` -> `git commit` (one reviewed commit, message from the task) ->
  `git branch -D task/<id>`.
- **Serialize** - open off current `main` and integrate before the next task, so the merge is a
  conflict-free fast path.
- **Conflicts** - the Director resolves only non-source (docs/config); a **source** conflict is a
  source edit: abort and route it back via a merge task-file, never hand-resolved.
- **Doc/config-only Director changes** commit straight to `main` (no branch).
- **Pushing is the user's.** Release tagging is the versioning close-gate: `Docs/Versioning.md` +
  `Tools/Check-ReleaseTag.ps1` (run at a user-triggered release, not between releases).
- **Handoff-id mint** (PS 5.1, UTC - run the clock, never type from memory):
  `(Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss')`.

## clangd / compile-commands

MSVC emits no `compile_commands.json`; `Tools/gen-compile-commands.ps1` reconstructs it (clang-cl
driver) for the DsonParser + DsonTest2 targets - regenerate after adding/removing a `.cpp` or
changing includes/defines. `.clangd` at the repo root skips indexing the vendored RapidJSON tree.
