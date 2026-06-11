# DSON Parser Audit Prompts

Use these prompts with Codex, Rider AI, or another LLM when you want a focused
review of one parser area. Start each audit by asking the model to read
`docs/dson-parsing-overview.md`, then inspect only the relevant implementation
files.

## General Parser Audit

```text
Using docs/dson-parsing-overview.md as the map, audit the DSON parser for
behavior gaps, ambiguous API semantics, and missing tests. Focus on DsonTypes.h,
DsonTypes.cpp, DsonParserAPI.h, and DsonParserAPI.cpp. Do not suggest style-only
changes. Report findings with file/line references, severity, and a short
explanation of the importer-visible impact.
```

## Scene vs Library Data

```text
Audit whether the parser and C API correctly distinguish scene.* instance data
from *_library definition data. Use docs/dson-parsing-overview.md first, then
check DsonTypes.h, DsonTypes.cpp, DsonParserAPI.h, and DsonParserAPI.cpp. Look
for index confusion, missing accessors, misleading comments, or cases where
scene instance values could be mistaken for library defaults.
```

## Geometry And Polylist

```text
Audit the geometry parsing path. Verify how vertices, polygons, polylist faces,
face offsets, polygon groups, material groups, vertex_count, polygon_count, and
default_uv_set are parsed and exposed. Check for malformed DSON shapes, variable
face length edge cases, count/value mismatches, and C API index behavior.
```

## UV Sets

```text
Audit the UV set parsing and API. Verify how UV coordinates, vertex_count, and
polygon_vertex_indices are handled, especially sparse [face, corner, uv_index]
triplets. Check whether the identity-default mapping is documented clearly,
whether legacy flat data behaves safely, and whether consumers have enough data
to expand UVs per face corner.
```

## Materials And Images

```text
Audit material and image parsing. Verify top-level diffuse handling, extra[]
shader type capture, studio_material_channels parsing, source-order channel
preservation, scalar/color/image fields, scene material differences, and
post-parse image_url to texture_path resolution. Look for dropped channels,
ambiguous shader semantics, missing fallbacks, or lifetime issues in API returns.
```

## Skin Binding

```text
Audit the skin binding pipeline end to end. Verify how DSON joint->vertex
weights are parsed from node_weights and local_weights, stored raw, exposed by
raw skin APIs, inverted into vertex->bone influences, sorted, normalized, capped,
and renormalized. Check for modifier index confusion, missing bounds checks,
parallel array mismatches, zero-weight behavior, and importer-facing edge cases.
```

## Morph Targets

```text
Audit morph target parsing and API access. Verify filtered morph indexing,
channel label/name behavior, sparse position deltas, sparse normal deltas, and
GetMorphGeometryId parent URL fragment extraction. Check for raw modifier index
confusion, missing geometry association cases, and sparse delta edge cases.
```

## API Ownership And Error Semantics

```text
Audit the public C ABI for ownership and error semantics. Verify opaque handle
lifetime, parser-owned const char* return values, scratch-string behavior,
invalid handle/index return conventions, last-error behavior, Clear/Destroy cache
reset behavior, and whether comments match implementation. Report any places
where a C or C# caller could retain invalid pointers or misinterpret failure.
```

## Unknown-Key Diagnostics

```text
Audit unknown-key diagnostics. Verify that known-key sets match the parser's
intended v1 support, that unknown keys are recorded in useful contexts, and that
recognized-but-unparsed fields are documented honestly. Check whether a clean
unknown-key report could be misread as full semantic support.
```

## v2 Formula Planning

```text
Using DsonParser_Roadmap.md and docs/dson-parsing-overview.md, audit the planned
formula parsing work before implementation. Identify the DsonTypes.h/.cpp model
changes, C API additions, unknown-key changes, and tests needed to support
formula operations such as push url, push val, mult, div, add, sub, pow, and
spline_tcb. Do not implement; produce a concrete implementation checklist.
```

## Accessor Fan-Out (C ABI Maintenance Tripwire)

```text
This is a maintenance-health tripwire, not a coverage audit: it watches whether
the flat C ABI's per-field accessor fan-out is outgrowing the model it exposes,
so the threshold for a struct-returning C ABI is caught on the trend rather than
after the fact. Start with docs/dson-parsing-overview.md (API Ownership section)
and docs/code-review-rules.md (R1 per-family return-value contract, R2
API-change-is-breaking rule, R3 DRY helpers). Inspect DsonParserAPI.h,
DsonParserAPI.cpp, DsonTypes.h, and CHANGELOG.md. Do not implement; report
findings + a verdict.

Measure (countable):
1. Accessor/field ratio. Count exported DsonDocument_* functions in
   DsonParserAPI.h vs the data-carrying members across the DsonTypes.h structs.
   Flag if the function count is growing faster than the model (super-linear
   fan-out). Reference point: ImageLayer is one struct exposed via 14 accessors.
2. Mirrored families. Find leaf-struct accessor families re-emitted under more
   than one parent path for the SAME physically-shared data -- e.g. the per-layer
   compositing family exposed both as DsonDocument_GetSceneMaterialChannelLayer*
   and DsonDocument_GetImageLayer* (both read the one Image::layers stack).
   EXCLUDE the deliberate library-vs-scene instance pairs (GetMaterial* /
   GetSceneMaterial*, GetMaterialChannel* / GetSceneMaterialChannel*,
   GetModifierFormula* / GetSceneModifierFormula*, the Modifier channel-dial
   pair, the material-groups pair): those mirror the DSON library/instance
   duality over DIFFERENT data populations and are mandated by the Scene-vs-
   Library design, not fan-out. Count only genuine same-data mirrors, and list
   how many surfaces reach each. Baseline 2026-06-11: 1 (ImageLayer).
3. Max index arity. Find the highest number of index parameters on any accessor
   (today: 3 -- material, channel, layer). Flag any accessor at 4+.

Judge (qualitative):
4. R1 slips on the newest accessors. For the most recently added families, check
   the R1 family return-value contract holds: correct sentinel for the family
   ("" / 0 / 0.0 / false / -1), bounds-check before access, and the
   count-accessor and value-accessor agreeing on what is valid. Recurring slips
   on freshly added accessors mean the flat pattern is generating this bug class
   by construction.
5. Ceremony vs logic. Across recent CHANGELOG.md releases, judge whether the bulk
   of each release is mechanical accessor/header/version fan-out rather than
   parsing logic. (Do NOT browse .handoff/ -- it is off-limits per CLAUDE.md; use
   CHANGELOG.md and the source.)

Verdict (recurrence, not magnitude, is the trigger):
- GREEN: no mirrored family beyond the one known (layer compositing), max arity
  <= 3, no R1-slip cluster on new accessors.
- YELLOW: a SECOND leaf family gets hand-mirrored across surfaces, OR an accessor
  reaches 4 indices, OR R1 slips recur on new accessors.
- RED: multiple newly mirrored families / pervasive fan-out across the surface.

If YELLOW/RED and the cause is leaf-heavy structs reached from multiple paths,
the bounded remedy is a struct-returning C ABI for those sections (return a small
POD per channel/layer): it keeps ABI safety and SemVer-over-C-ABI stability while
cutting both call count and accessor boilerplate. Do NOT reach for it if the real
pain is instead ABI instability (a struct return makes that worse), data-shape
comprehension (fix dson-parsing-overview.md, not the API shape), or a recomputed
view (use a bulk accessor or a lazy cache). Report the metric values, the
GREEN/YELLOW/RED verdict, and the recommendation with file/line references.
```
