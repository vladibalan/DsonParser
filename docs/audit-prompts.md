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
1. Accessor/field ratio. Numerator = exported DsonDocument_* functions in
   DsonParserAPI.h. Denominator = data-carrying members across the DsonTypes.h
   structs. Flag only SUPER-LINEAR growth -- the function count outpacing the
   model. Reference point: ImageLayer is one struct exposed via 14 accessors.
   Two traps live in this metric; follow the method rather than improvising.
   a. Rebuild the whole series from git. Do NOT diff today's ratio against a
      ratio quoted in a past run. For each commit touching DsonParserVersion.h,
      read that revision's version string and count BOTH sides at that same
      revision. The trend is the finding; a lone ratio is not.
   b. Numerator: count lines matching ^DSONPARSER_API.*\bDsonDocument_ .
   c. Denominator: count field lines in DsonTypes.h -- but STRIP // comments
      BEFORE excluding lines containing "(". Many field lines carry comments with
      parens, so filtering the raw line silently drops them, and the bias GROWS
      over time (newer fields have wordier comments) -- manufacturing a rising
      ratio out of nothing. Also exclude enum lines and method declarations.
      Hand-enumerate a struct or two to cross-check before trusting the series.
   Known-good anchors (corrected 2026-07-16; these SUPERSEDE any earlier recorded
   figure): 1.0.0 = 182/119 = 1.53, 1.4.0 = 227/143 = 1.59, 2.5.0 = 1.65 (peak),
   2.17.0 = 317/200 = 1.58. Since 1.4.0: exports +39.6%, model +39.9% -- linear.
   If your 2.17.0 denominator is not 200, your field count is wrong; fix it
   before reading the trend.
   Trap to know about: the 2026-06-11 run recorded "~229 functions vs ~200 model
   leaves -> ~1.1:1". The model at 1.4.0 was actually 143, so the true ratio then
   was 1.59 and the recorded 1.1 is not a valid baseline. ~200 is TODAY's field
   count, so checking a correct 1.58 against that stale 1.1 shows a fake 45% jump
   and a false RED. This metric has never trended badly.
2. Mirrored families. Find leaf-struct accessor families re-emitted under more
   than one parent path for the SAME physically-shared data -- e.g. the per-layer
   compositing family exposed both as DsonDocument_GetSceneMaterialChannelLayer*
   and DsonDocument_GetImageLayer* (both read the one Image::layers stack).
   EXCLUDE families that mirror a SCHEMA over DIFFERENT data populations: those
   are mandated by the Scene-vs-Library design (R6.3) and the no-cross-section-
   merge rule (R6.4), not symptoms of fan-out. Excluded, do not re-litigate:
     - The library-vs-scene instance pairs: GetMaterial* / GetSceneMaterial*,
       GetMaterialChannel* / GetSceneMaterialChannel*, GetModifierFormula* /
       GetSceneModifierFormula*, the Modifier channel-dial pair, the
       material-groups pair, and GetModifierParent / GetSceneModifierParent
       (2.11.0 / 2.17.0).
     - GetGeometryMaterialUVAssignment* (geometry_library) vs
       GetSceneNodeShellMaterialUVAssignment* (2.13.0, scene.nodes shell extra):
       one MaterialUVAssignment leaf, but two independent vectors sourced from
       two different sections. R6.4 FORBIDS combining them, so two families are
       required rather than duplicated.
     - The DSON rigidity_group schema, modeled twice: flattened onto Node as
       rigid_follow_* (2.8.0, from node extra[]) and as GeometryRigidityGroup
       (2.10.0, from geometry.rigidity). Same schema, different populations ->
       not a same-data mirror. Worth NOTING as schema duplication if a third
       copy ever appears, but it does not count toward this metric.
   Count only genuine same-data mirrors, and list how many surfaces reach each.
   Baseline 2026-06-11: 1 (ImageLayer). Re-confirmed 2026-07-16 @ 2.17.0: still
   1 -- no library-side GetMaterialChannelLayer* exists.
3. Index arity vs data depth. Fan-out shows up as an index the DATA does not
   justify -- not as depth the DSON genuinely has. For the highest-arity
   accessors, compare each one's index count against the nesting depth of the
   DSON structure it reads, and flag only an accessor whose arity EXCEEDS that
   depth (an extra index existing because the flat ABI re-emits a leaf under an
   additional parent path). Count index parameters only: an int that configures
   rather than indexes does not count -- GetVertexBoneInfluenceCapped's
   maxInfluences makes it 3 indices, not 4.
   SANCTIONED, do not re-flag: Get{Modifier,SceneModifier}FormulaOperationVal-
   ArrayElement sits at 4 (modifier -> formula -> operation -> val_array element,
   @since 2.1.0) because the DSON is genuinely 4 deep. No ABI shape removes an
   index there, and a POD return cannot carry the variable-length val_array, so
   the struct remedy does NOT apply -- a bulk accessor is the shape if it ever
   hurts. (This criterion previously read "flag any accessor at 4+", which fired
   on that irreducible nesting from 2.1.0 onward and could never be actioned.)
   Baseline 2026-07-16 @ 2.17.0: max sanctioned arity 4; ZERO accessors exceed
   their data's depth. A 5-index accessor degrades caller ergonomics regardless
   of nesting, so it stays a backstop trigger in the verdict below.

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
- GREEN: no mirrored family beyond the one known (layer compositing), no accessor
  whose index arity exceeds the depth of the DSON it reads, no R1-slip cluster on
  new accessors.
- YELLOW: a SECOND leaf family gets hand-mirrored across surfaces, OR an
  accessor's index arity exceeds the depth of the DSON it reads, OR any accessor
  reaches 5 indices (backstop -- sanctioned nesting or not), OR R1 slips recur on
  new accessors.
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
