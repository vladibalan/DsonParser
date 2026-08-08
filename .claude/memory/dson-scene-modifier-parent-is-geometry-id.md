---
name: dson-scene-modifier-parent-is-geometry-id
description: "In a DAZ scene .duf, scene.modifiers[].parent points at a node's GEOMETRY id, not its node id; attribute dials via a {geometry id -> node} map, and read presentation.type via the node url fragment."
metadata: 
  node_type: memory
  type: reference
  originSessionId: 1a4c55bc-a1a5-45b0-b16f-65bd752eecdc
---

DSON scene-structure gotcha (ground-truthed on `Test01.duf`, 2026-07-11): each scene node carries a
node `id` AND a nested geometry with its own `id` -- often node-id + `-1`, but NOT always a suffix.
Examples: node `Genesis9` -> geometry `Genesis9-1`; node `Genesis9Eyelashes` -> `Genesis9Eyelashes-1`;
wearable node `Genesis9_JewelBikini._Bottom_66554` -> geometry `Genesis9_JewelBikini._Bottom`; hair
node `G9EyebrowFibers` -> geometry `G9Eyebrows_FS06_Thn` (no suffix relation at all).

**`scene.modifiers[].parent` references the GEOMETRY id, not the node id.** So dial / skin-binding
attribution must map `parent -> owning node` via a `{geometry id -> node}` map (`GetSceneNodeGeometryId`),
NOT by node id and NOT by a uniquifier-suffix string heuristic (that only coincidentally catches
suffix-shaped geometry ids like the bikini's, and misses `G9Eyebrows_FS06_Thn`, `Genesis9-1`, etc.).

`#Genesis9-1` is the base figure's GEOMETRY, NOT an HD-instance node -- do not try to "group" it as a
node. Only the figure's own top-level dials (character controller, FACS, script-loaders, push) parent
to the NODE id `#Genesis9`.

For a scene node's `presentation.type`, resolve via the NODE url fragment (`GetSceneNodeUrl`, whose
`#fragment` is the node id) into the referenced DSF's node_library -- the geometry url's fragment is
the geometry id and won't match a node.

Surfaced by the scene-manifest importer's attribution + presentation.type bugs (8/17 unattributed,
all presentation.type empty) on the first user runtime run; fixed round 3. See
[[load-composed-scene-bake-arc]], [[ground-truth-before-fixing]].
