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
