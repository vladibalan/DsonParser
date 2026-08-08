# Versioning Module - SemVer over the published surface (framework core, code layer)

A **verbatim framework core**: domain-free, byte-identical across consumers. **Dormant / opt-in
(DecisionLog D17):** a project stands up versioning when it gains a **downstream consumer** - release
carriers are dead weight in a consumer-less project. Until then this module is inert. It carries
**COD-11**: how the project tells a downstream consumer what it is and what changed. Cold/on-demand. The
project's concrete carriers and its surface definition are local bindings.

## What is versioned - the published consumer surface

Version the **published surface a consumer binds**, not any one consumer's needs - the public
API/symbols (their fields, enumerators, and call/data contracts) plus any documented emitted-output
shape a consumer relies on. The project declares its own surface in a local binding; everything else
(internal implementation, private tiers) is not surface, and changing it never breaks a consumer.

## SemVer - the boundary is surface compatibility

`MAJOR.MINOR.PATCH`:

- **MAJOR** - breaking: a removed / renamed / re-signatured public symbol, a changed call or data
  contract, or a breaking output change (a moved / renamed / removed emitted artifact a consumer relies
  on).
- **MINOR** - additive and back-compatible: a new public symbol or optional field, or a new capability
  even one no consumer yet binds (capability milestones read as MINOR; they never under-signal
  compatibility).
- **PATCH** - a fix or internal change with the surface unchanged and no new capability.
- **No bump** - comment / doc / config-only edits with no functional effect: not versioned, no changelog
  entry.

## Carriers

Three carriers announce a change; the exact form is the project's, stood up at opt-in:

- **The SemVer source of truth** - one authoritative version literal (a version-macro header, or a
  manifest version field the platform already exposes at runtime). Do not add a second accessor that
  duplicates one the platform already provides.
- **A `vX.Y.Z` git tag** on the release commit - the one release carrier **not in the tree**, invisible
  to `git diff`, so the most easily missed.
- **A CHANGELOG** at the repo root - what changed each release, and (if the project vendors an upstream)
  the upstream version that release depends on.

**Baseline `1.0.0`** labels the current tree as the consumer baseline; pre-versioning history is not
retro-numbered - the git log and decision log already carry it, and a pinning consumer needs one anchor,
not a reconstructed history.

## COD-11 - Stage every surface change now; bump only on the user's request

**Version bumps are user-initiated only** - the Director never cuts a release or picks a number unbidden.
Two phases:

### Every surface-touching merge - stage under `[Unreleased]`

A change that touches the consumer surface (or ships a new capability) records it in the **same change**,
without bumping the version source or tagging:

- Append **one sigil-prefixed line** (`+` added, `~` changed, `-` removed/deprecated, `!` fixed) under a
  top `## [Unreleased]` heading in the CHANGELOG (the same wording a released entry would carry). The
  sigil signals severity; the aggregate MAJOR/MINOR/PATCH class is settled at release, not now.
- Touch **neither** the version source **nor** any tag.

The Director makes this edit at the squash-merge, assessing surface impact from the diff (the Implementer
edits no version files - the source-only boundary, `AgentWorkflow.md`). A surface change merging with
**no** `[Unreleased]` line is stale-orientation drift (TXT-7).

### On the user's request - finalize the release (Director)

1. **Classify** the accrued `[Unreleased]` set MAJOR/MINOR/PATCH (the aggregate = its most significant
   change). Absent a user-named number, the Director derives it from the scheme and states it for
   confirmation - it never picks one unbidden.
2. **Fold `[Unreleased]`** into a dated `X.Y.Z - date - CLASS` heading (prepend the upstream-dependency
   line if the project vendors one, COD-10).
3. **Bump** the version source of truth.
4. **Tag** the release commit `vX.Y.Z`. The user pushes commit + tag (the Implementer never runs git;
   pushing is the user's).

### The close-gate (Director, at the release only)

Runs only at a user-triggered release - nothing is tagged between releases. Because the tag is invisible
to `git diff`, confirm the version bump and the folded heading are in the diff, then run the release-tag
check tool (code-layer tool core) - it cross-checks the version source, the CHANGELOG top heading, and
the `vX.Y.Z` tag, and names the carrier that is missing or drifted.

## How a consumer takes up a change

**Pin** the version / tag validated against (and re-vendor any upstream at the tag the CHANGELOG entry
declares - two differently-versioned copies of one dependency are a fatal duplicate load); **read** the
CHANGELOG (one targeted read); **re-wire** per each entry - a MAJOR entry is the signal to adapt before
uptaking. This is the same-change pin discipline (DecisionLog D3).
