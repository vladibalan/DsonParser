# DsonParser - Reuse Patterns (capability -> existing seam)

Indexed by **what you are trying to do** - the lookup a Director runs *before* scoping a feature so
a look-alike task **extends an existing seam** instead of reinventing one (COD-1). Find the nearest
row, then read that seam's home file (`Docs/dson-parsing-overview.md` for layout). This doc points,
it does not restate. Cold/on-demand; grow it as seams emerge.

**No row matches your shape?** Either the seam hides under another name (read the overview's file
map before concluding it is absent) or a fresh design genuinely is warranted - in which case say so,
with the reason, in the task-file rather than reinventing silently.

## Capability -> seam

| If you are doing... | Reuse this seam | Owner |
| --- | --- | --- |
| Element access in a C-ABI accessor | `Doc(handle)` + `At(coll, idx)`; nest `At` for depth | `DsonParserAPI.cpp` |
| A new accessor family (>= 3 near-clones) | one shared helper, as the material-channel and node-transform families do | `DsonParserAPI.cpp` |
| Leaf field extraction from JSON | `ParseMember(json, "key", field)` | `DsonTypes.cpp` |
| Array-or-`{count,values}` unwrap | `GetValuesArray(container, "key")` | `DsonTypes.cpp` |
| Array-of-objects section parse | `ParseObjectArray(container, "key", vec, unknownKeys)` | `DsonTypes.cpp` |
| Parsing a new key in a known context | add it to that context's `static const knownKeys` set | `DsonTypes.cpp` |
| Announcing a C-ABI change | the versioning carriers | `Docs/Versioning.md` |

Exception rows: `vertices` / `polylist` legitimately also read `"count"` beside `GetValuesArray`;
an unlisted new key otherwise false-positives in the unknown-key audit (`TrackUnknownKeys`).

## Shared leaf helpers (the COD-1 inventory)

- `DsonParserAPI.cpp`: `Doc` (null-safe handle -> document), `At` (bounds-checked element access,
  returns `nullptr`) - **correctness-critical**: never re-inline the bounds guard.
- `DsonTypes.cpp`: `ParseMember` (leaf extraction), `GetValuesArray` (array-or-object unwrap),
  `ParseObjectArray` (section-array parse), `TrackUnknownKeys` (unknown-key audit trail).
- Re-introducing the inline patterns these replaced (hand-rolled `if (!handle)` / bounds guards,
  double-hash-lookup parses, reserve-loop-push blocks) is a COD-1 finding.
