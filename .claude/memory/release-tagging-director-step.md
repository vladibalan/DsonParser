---
name: release-tagging-director-step
description: DsonToUnreal vX.Y.Z git tag is a Director step at squash-merge and is EASILY MISSED — confirm it every release.
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 6f348141-0216-49ac-92c7-852b85f624c6
---

Each DsonToUnreal release commit gets a **lightweight** `vX.Y.Z` git tag. Per `Docs/Versioning.md`
step 4 this is the **Director's** job at squash-merge (the Implementer never runs git; the user
pushes the tag). The consumer (DsonArtisan) pins via this tag, so a missing tag breaks the pin.

**Why:** the doc explicitly flags the tag as "easily missed" — structurally, it's the one carrier
that is NOT an in-tree file, so it's invisible to `git diff`, the build, and the R12 review (the three
signals that catch the VersionName bump + CHANGELOG entry), and it crosses the Implementer→Director
handoff seam. This is a **recurring multi-Director miss**, not a one-off: the user confirmed (2026-06-24)
they have had to remind *previous* directors too — the user keeps ending up as the forcing function,
which is exactly the failure the Versioning.md close-gate is meant to prevent. All current tags
(through 4.0.0) now exist; the gap was adherence, not the doc.

**How to apply:** make tagging part of the squash-merge close checklist, alongside the
CHANGELOG/Roadmap/version bump — not an afterthought. Immediately after the release commit lands on
main: `git tag vX.Y.Z <release-commit>` (lightweight, no `-a`) and confirm `git tag --list vX.Y.Z`.
The user pushes commit + tag (e.g. `git push origin main vX.Y.Z`). See [[dson-plugin-versioning-contract]].

**Now mechanized (2026-06-24):** `Tools/Check-ReleaseTag.ps1` (documented in Docs/Tooling.md) is the
close-gate — `pwsh -File Tools/Check-ReleaseTag.ps1` checks the current VersionName has its tag (exit 1
on a miss, prints the exact `git tag <detected-commit>` command); `-Audit` cross-checks every CHANGELOG
heading. Run it before reporting a surface-touching task done. Its `-Audit` already caught a real
historical gap: **v1.1.0 had never been tagged** — now created at release commit `8d4131e` (the tool
itself shipped in commit `0d21f2d` on main, doc/config-only, no version bump; both local/unpushed — user pushes).
