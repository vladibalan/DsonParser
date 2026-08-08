---
name: dson-catalog-metadata-fields
description: "where DAZ DSON declares content type / display label / geograft signal, and the 1.5.0 parser accessors that expose them faithfully"
metadata: 
  node_type: memory
  type: project
  originSessionId: 7efd5616-9174-4235-9ad2-8b0d207697d2
---

For a faithful asset catalog of installed `.duf`/`.dsf`, three declared facts and where
they actually live — ground-truthed against real files, not abstractly ([[ground-truth-before-fixing]]):

- **Content type = `presentation.type`** (there is NO literal `content_type` key).
  **Display label = `presentation.label`.** The `presentation` block
  (`{type,label,description,icon_large,colors}`) sits **per library item**, not at document
  root, and appears in many contexts (node_library, modifier_library, even material
  channels — mostly empty there). The asset's content type is on its **defining** library
  item: node item for figures/clothing/hair/props (`"Follower"`, `"Wardrobe/Clothing"`),
  modifier item for shapes (`"Modifier/Shape"`). Presets (`.duf`) usually have empty/absent
  presentation → unknown.
- **⚠️ Geograft signal: key-presence is WRONG.** DAZ writes an empty `"graft": {}` on
  NON-graft meshes — base figures, **G9 Eyes (uses `rigidity`)**, **G9 Eyelashes
  (conforming)**. A real geograft has a **populated** graft with `vertex_pairs` (e.g.
  `Genesis9FemaleGenitalia.dsf` → 84 pairs). The faithful signal is "graft present AND
  non-empty `vertex_pairs`."

Exposed faithfully **per item** in **1.5.0** (additive MINOR; R6.4 / [[parser-faithful-no-cross-section-merge]]
— the parser does NO classification, document-level content-type resolution, or merge; the
importer maps `presentation.type` to its taxonomy and selects the defining item):
`DsonDocument_Get{Node,Modifier}Presentation{Type,Label}` + `DsonDocument_GetGeometryIsGraft`.
Verified: test.dsf `"Modifier/Shape"`; Genesis9.json base `is_graft=false`, node[0] `"Actor"`;
genitalia `is_graft=true`, node[0] `"Follower"`. The Importer's library catalog is the
consumer ([[agnostic-plugin-architecture]]); this was an inbound Importer→Parser feature
request.

**Why:** the geograft empty-`{}` gotcha and the presentation-per-item location are
non-obvious and took disk ground-truth; they'll matter when the Importer builds the catalog.
**How to apply:** in the Importer, read presentation per library item and pick by
`asset_info.type`; treat `GetGeometryIsGraft` as already-populated-only. Don't re-derive the
graft signal from key-presence, and don't expect a single document-level content_type.
