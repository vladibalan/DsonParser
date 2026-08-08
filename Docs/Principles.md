# DsonParser - Governing Principles

The project's intrinsic purpose, stated once - what it is *for*. **The Roadmap is subordinate to
this:** when an open item conflicts with a principle here, the principle wins and the item changes.
Principles are *requirements* - how you work lives in the disciplines and doc-rules cores; the
*why* behind a shipped decision lives in `Docs/DecisionLog.md`; durable facts in
`Docs/Reference.md`; current status in `Docs/Roadmap.md`. Approved by the owner 2026-08-08.

## P1 - Faithful, non-interpretive parsing

Report each DSON section as the file states it. Interpretation - merging overrides, evaluating
formulas, collapsing instances onto definitions - is the consumer's job, computed from faithful
inputs. (Rule form: COD-101 in `Docs/Rulebook.md`.)

## P2 - Permissive ingest; real content loads

Missing optional fields keep defaults, malformed entries skip with a warning, unknown keys are
recorded for audit - never rejected. Hardening into validation is a semantic shift, not a fix.
(Rule form: the COD-4 permissive-ingest clause.)

## P3 - One published surface, always announced

Consumers bind only the flat C ABI (`DsonParserAPI.h`, the single export header); internals - STL,
vendored RapidJSON, the C++ model headers - never cross it. Every surface change ships its
announcement in the same change: version macros, `@since` tags, CHANGELOG (`Docs/Versioning.md`).

## P4 - Consumer-agnostic; widen just-in-time

No bespoke logic for any one downstream (UE included); capability widens additively when a
concrete consumer need lands, as additive-MINOR accessors.
