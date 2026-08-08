---
name: implementer-concurrent-run-git-check
description: "Shared working trees can be branch-switched by a concurrent run mid-session: Implementer seat — vanished branch / clean tree means a parallel run already merged (git show before assuming loss); Director seat — a task branch can appear and be checked out under you, so verify the current branch immediately before any commit."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 7fa6d0d6-8c21-4ac9-9842-23fbac84acdf
---

When working an Implementer task-file/branch and a git check (status/branch/log) shows the
task branch is gone, or the tree is unexpectedly clean on `main`, don't treat it as lost or
conflicting work. It most likely means the user launched a **second, concurrent Implementer
run on the identical task-id** (possibly a different model/agent) that finished first, was
Director-verified and squash-merged, and had its branch deleted — all while this session kept
working. Edit/Write tool calls act directly on the filesystem and are unaffected by a
concurrent branch delete/switch, so this session's own edits keep succeeding throughout; they
just end up superseded rather than destroyed.

**Why:** discovered 2026-07-09 on DsonToUnreal `task-20260709-104423-material-set-patch`. This
session (Sonnet 5) did a slow, methodical trace-then-fix and wrote a "blocked" feedback file
halting on one of two defects. A `git log`/`git status` check right after showed `main` already
several commits ahead — `git show 5417caa -- <file>` proved that commit's diff was **byte-for-
byte identical** to this session's own edits (co-authored-by trailer named a different model,
"Claude Fable 5" — almost certainly the parallel run, or the Director's own commit identity, not
evidence of independent re-derivation). A further commit (v6.13.0) had *also* already resolved
the exact design halt this session reported blocked — using a mechanism (reusing the existing
`bIsGeometryShell` flag from an earlier, unrelated feature) this session hadn't considered, via
a design insight only the user could supply. See [[material-set-import-research]] for the arc.

**Director-seat mirror (2026-07-10, DsonArtisan):** the same phenomenon runs the other
direction. A Director session verified `main` was checked out, worked for a few minutes, then
committed — and the commit landed on `task/20260710-061904-rigidfollow-runtime-rotation`: the
user had launched an Implementer run mid-conversation, which checked out a fresh task branch in
the same working tree. Recovery (tree never touched): confirm the stray commit's parent is
`main`'s tip and the branch has no other commits, then `git branch -f main <sha>` — a pure
fast-forward with no checkout; the task branch drops back to zero-ahead so the eventual
squash-merge diff stays purely the Implementer's work.

**How to apply:** in any shared tree (DsonArtisan, DsonToUnreal), re-check the current branch
in the same breath as committing (`git branch --show-current` immediately before, or read the
`[branch sha]` line git prints after commit) — a status check from minutes earlier is stale.
Before reporting anything as lost, redundant, or in conflict, run
`git show <suspect-commit> -- <file>` and compare it to what you actually wrote — if identical,
say plainly that the work was accepted via a faster parallel path rather than re-doing or
second-guessing it. If a halt/design-fork you reported was since resolved upstream, surface the
newer resolution to the user (it may reveal reusable existing machinery, or other domain context,
that the original halt reasoning missed) instead of silently standing by your own now-superseded
write-up, and don't assume your own single-session halt was the last word.
