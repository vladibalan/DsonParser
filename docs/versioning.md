# DsonParser — Versioning & Change Announcement

How DsonParser tells its upstream consumers *what it is* and *what changed*. The
primary consumer is an **LLM agent in a separate, agnostic repo** (e.g. the
DsonToUnreal plugin) that has only the **shipped artifacts** — not this repo's
source tree, git history, or [`DsonParser_Roadmap.md`](../DsonParser_Roadmap.md).

## The carrier contract (what ships, what doesn't)

A change announcement is only useful if it travels with the artifact the consumer
actually receives. DsonParser announces through four carriers:

| Carrier | Answers | Consumed by |
|---|---|---|
| **Version macros** in `DsonParserVersion.h` (included by `DsonParserAPI.h`) | "Which version is this, at compile time?" | importer C++ (`#if DSONPARSER_VERSION_*`) |
| **`DsonParser_GetVersion()`** (C ABI) | "Which version am I linked against, at runtime?" | importer code; agents via the test harness |
| **`@since x.y.z` tags + header banner** in `DsonParserAPI.h` | "Which symbols are new, and what's the headline?" | the agent reading the header (zero extra reads) |
| **`CHANGELOG.md`** (repo root, ships beside header/DLL) | "What changed each release, and what do I wire up?" | the agent, one targeted read |

**Not carriers:** git history and `DsonParser_Roadmap.md` live only in this repo
and never reach the consumer — never rely on them to announce a change.

## Versioning scheme — SemVer with C-ABI semantics

`MAJOR.MINOR.PATCH`, where the boundary is **binary (ABI) compatibility of the
flat C API**:

- **MAJOR** — a *breaking* ABI/behavior change: a removed/renamed export, a
  changed signature, a changed return-value contract, or a semantic change that
  can break an existing caller. (This is the R2 case in
  [`code-review-rules.md`](code-review-rules.md).)
- **MINOR** — an *additive*, binary-compatible change: a new exported function or
  a new optional field, with every existing symbol and contract intact. New
  exports leave the consumer's existing import table valid. (The image
  `map_size`, formula, and LIE-layer surfaces were all MINOR-class additions.)
- **PATCH** — an internal fix with **no** change to the published surface
  (`DsonParserAPI.h` byte-identical): a parser bug fix, a perf change, etc.
- **Documentation-only** (no bump) — a comment/whitespace-only edit to
  `DsonParserAPI.h` that changes no symbol, signature, `@since` value, or behavior
  (e.g. tightening the header banner): **not** version-bumped, **no** CHANGELOG
  entry. It breaks PATCH's byte-identical clause but has no functional effect, so
  it sits outside MAJOR/MINOR/PATCH — nothing a consumer wires up changed.

### Capability milestones ≠ SemVer

`DsonParser_Roadmap.md` tracks capability *epochs* ("v1", "v2 formulas"). These
are **not** library versions. v2's formula work shipped as **additive MINOR**
bumps within `1.x` — adding a capability does not imply `2.0.0`. A `2.0.0` comes
**only** from a breaking ABI change.

## Single source of truth

`DsonParser/DsonParserVersion.h` holds the canonical macros; everything else
mirrors them (format shown — the live values are whatever the header currently
declares, illustrated here at the `1.0.0` baseline):

```c
#define DSONPARSER_VERSION_MAJOR  1
#define DSONPARSER_VERSION_MINOR  0
#define DSONPARSER_VERSION_PATCH  0
#define DSONPARSER_VERSION_STRING "1.0.0"
```

`DsonParser_GetVersion()` returns `DSONPARSER_VERSION_STRING`; the `CHANGELOG.md`
top heading leads with it; the header banner states the same string. On every
release, the macros and the CHANGELOG heading are the two human-touched points —
keep the heading's leading version equal to the macros.

### Naming note

The library-version accessor is `DsonParser_GetVersion()` (global `DsonParser_*`
family, like `DsonParser_GetLastError`). Do **not** confuse it with the existing
`DsonDocument_GetFileVersion()`, which returns the *parsed asset's* `file_version`
field — a property of the loaded DSON file, not of this library.

## Baseline

**1.0.0** is the first *versioned* release: it labels the tree as of that release
(~180 C ABI functions, including the post-v1-epoch additive surfaces folded in by
then). Pre-versioning history is **not** retro-numbered — it predates any shipped
version. Surfaces added *since* 1.0.0 ship as additive MINOR bumps (1.1.0+), so
the live total is higher than the baseline ~180; those changes are tracked per
release in [`../CHANGELOG.md`](../CHANGELOG.md), while capability detail for the
baseline lives in `DsonParser_Roadmap.md`.

## Per-change workflow

Any change to the published surface (`DsonParserAPI.h`) must, in the same change:

1. **Classify** it MAJOR / MINOR / PATCH per the scheme above.
2. **Bump** the macros in `DsonParserVersion.h`.
3. **Tag** each new symbol with `@since <new version>` and refresh the header
   banner's "what's new" line.
4. **Add a `CHANGELOG.md` entry**, newest first, under a heading that leads with
   `DSONPARSER_VERSION_STRING` (`version — date · CLASS`): name each new symbol
   with a one-line, sigil-prefixed semantic (`+` added / `~` changed / `-`
   removed-or-deprecated / `!` fixed). Lean format — no empty subsection
   scaffolding; the CHANGELOG is the only doc carrier that ships, so the repo's
   token-economy principle applies.

This is enforced as **R10** in [`code-review-rules.md`](code-review-rules.md). Per
the two-agent workflow the Director authors the CHANGELOG/policy text and the
Implementer makes the `DsonParserVersion.h` / `@since` source edits; the change
is not complete until both land.

## How an upstream agent consumes this

1. **Compat gate** — read the version macros (compile time) or call
   `DsonParser_GetVersion()` (runtime); refuse or branch on a MAJOR mismatch.
2. **What's new** — read `CHANGELOG.md` (one targeted read) for the per-release
   narrative, or read `@since` tags in `DsonParserAPI.h` while already in the
   header.
3. **Wire up** — each CHANGELOG / `@since` entry names the new C ABI symbols to
   bind.
