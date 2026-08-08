---
name: repo-tools-dir-and-git-add-verify
description: "DsonParser helper scripts live in Tools/ (canonical case since the 2026-08-08 LLMfrmw migration); core.ignorecase silently drops wrong-case git-add paths, so verify git show --stat after committing."
metadata: 
  node_type: memory
  type: project
  originSessionId: ad85a279-3012-4a7b-8ee2-e02f5cc2ea97
  modified: 2026-08-08T03:08:36.640Z
---

This repo keeps helper/tooling scripts in **`Tools/`** (canonical capital-T since the
2026-08-08 LLMfrmw migration renamed `tools/` -> `Tools/` and `docs/` -> `Docs/`; before
that date the tracked case was lowercase). `core.ignorecase=true`, so a wrong-case
pathspec **silently stages nothing** for that path: the surrounding
`git add ... && git commit` still returns success and commits the *other* files,
leaving the file untracked with no error.

**Why:** it cost a round-trip on 2026-08-07 — the release-tag setup commit landed
3 of 4 files; only `git show --stat` caught the missing `tools/Check-ReleaseTag.ps1`
(the `&&` chain reported success). A green-looking chain is not proof a file was
staged.

**How to apply:** use the exact tracked-case paths (`Tools/`, `Docs/`) in git commands
and docs; after any multi-file commit, verify with `git show --stat HEAD` (file count +
names) rather than trusting the add/commit chain. Link [[no-silent-fails]],
[[glob-spaced-path-plugin]], [[memory-maintenance-relocated]].
