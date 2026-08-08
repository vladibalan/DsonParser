---
name: user-decides-version-bumps
description: The user (not the Director) decides when to bump the DsonParser version; the Director never initiates a bump on its own.
metadata: 
  node_type: memory
  type: feedback
  originSessionId: d6c50261-e34d-4147-bdd9-789593b48d14
  modified: 2026-08-07T03:37:51.724Z
---

Rule: The **user** decides when a DsonParser version bump ships. The Director
never initiates one on its own initiative — neither by writing a
surface-touching (bump-forcing) task-file, nor by approving one an Implementer
proposes — and waits for the user's explicit call before handing off.

**Why:** Version bumps mark releases; downstream consumers (DsonToUnreal, and
via that DsonArtisan) wire against `DSONPARSER_VERSION_STRING` and the
`vX.Y.Z` tag. The release cadence — batching several surface changes into one
release vs. shipping each immediately, deferring a MAJOR while a MINOR path is
explored, etc. — is a user call, not something the Director should decide
unilaterally. Stated by the user on 2026-08-07.

**How to apply:**
- Don't include a version bump in any task-file spec unless the user explicitly
  asked for one; if a requested change would incidentally touch the published
  surface, name that fact (and the class it would force under R10) and ask the
  user for the version/release call before handing off.
- Don't approve an Implementer's returned bump the user hasn't authorized —
  treat it as a block, not a green-light.
- The mechanical execution once the user calls the bump (classify per
  `docs/versioning.md`; edit `DSONPARSER_VERSION_STRING` + `@since` +
  CHANGELOG; the `vX.Y.Z` tag at squash-merge; `tools/Check-ReleaseTag.ps1`) is
  unchanged and remains the Director's job — see [[release-tagging-director-step]].
- Related: this is the *initiation* half; the *what/why*-not-*how* discipline
  for cross-repo requests up the chain is [[parser-changes-route-through-user]].
