---
name: dson-animations-keyed-material-overrides
description: "DAZ stores figure/material INITIALIZATION data in scene.animations (esp. key 0), not just runtime animation; was root cause of G9 mouth/teeth \"metallic\"; parser EXPOSES it faithfully via DsonDocument_GetSceneAnimation* (1.2.0), importer CONSUMES key-0 in ApplySceneAnimationOverrides (merged + verified 2026-06-08); don't trust a \"textureless\" parser conclusion"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: be16e2b5-4b5c-4ad1-b9a7-9bb1466b27c0
---

**General principle (learned 2026-06-08):** DAZ uses the `scene.animations` array —
especially its **first key (time 0)** — to store figure/material *initialization*
values, not only runtime animation. Our import is one single figure-import step, so it
must read `scene.animations` and fold the key-0 values into that step; treating
animations as skippable runtime-only data silently drops real init data (textures,
channel values, figure setup). Confirmed instance so far — material channel bindings:

DAZ `preset_hierarchical_material` files (e.g. base G9 companion MAT presets like
`Genesis 9 Mouth MAT.duf`) **declare** their material channels in `scene.materials`
(often bare `{id,type}`, with a placeholder `diffuse` value) but **bind the real
values AND `image_file`s through a separate `scene.animations` array** of `{url, keys}`
entries. Each `url` is a DSON pointer such as
`<node>#materials/<matId>:?diffuse/image_file` (also `…?diffuse/value`,
`…?extra/studio_material_channels/channels/<Ch>/value`, `…/image_modification/*`) and
`keys` is `[[0, <value>]]`. So the authoritative Base Color texture/color, roughness,
translucency, etc. live in `animations`, **not** in the `materials` channels.

**This was the real root cause of the G9 companion Mouth/Teeth "metallic" bug**
(source-traced with the user 2026-06-08): those surfaces are not textureless — they
carry a `Genesis9_Mouth_D_1001.jpg` Base Color + a real roughness map (`_R_`) +
translucency, all bound via `animations` (verify the live key-0 values per file; the
plugin's `Docs/Reference.md` has the exact channel list). The earlier plan (reroute
"textureless" surfaces to `M_DazDefault`) was wrong.

**Resolution (split — both sides done 2026-06-08):** the parser **exposes
`scene.animations` faithfully** via the `DsonDocument_GetSceneAnimation*` family
(`Count` / `Url` / `ValueKind` / `Float` / `Bool` / `String` / `ColorR`/`G`/`B`), a raw
passthrough of every `{url, keys[0]}` entry; it does **NOT** apply these onto
`scene.materials` (the user rejected the merge approach on principle — see
[[parser-faithful-no-cross-section-merge]]). The **importer** consumes key 0 — landed
in DsonToUnreal `DsonMaterialBuilder::ApplySceneAnimationOverrides` (commit `e4002b7`):
after the base scene.materials pass it applies each key-0 `value`/`image_file` whose
matId matches and whose channel is a key in the active mapping table. **Verified 2026-06-08 (Nancy G9):
texture + UVs correct; residual over-shininess is a separate PBRSkin-master issue (Roadmap
Known issues).** The eyes companion needs more — its url
`<matId>` is percent-encoded + suffixed vs `GetSceneMaterialId` (url `EyeMoisture%20Left`
vs id `EyeMoisture Left-1`), so the override is a no-op there until UrlDecode + suffix
reconciliation lands (slice #3); details in the plugin's `Docs/Reference.md` +
`Docs/DecisionLog.md`.

**Why:** trusting the parser's exposed channels / the importer's output as the DAZ
ground truth produced a confident-but-wrong diagnosis twice (me + a previous Director);
the human had to untangle it.

**How to apply:** when a texture looks "missing" or a surface "textureless," do NOT
conclude from parser/importer output — open the actual DSON source and check
`scene.animations` for keyed overrides (`?…/image_file`, `?…/value`). Note `image_file`
can appear only as a path segment *inside an `animations` `url` string* plus a `keys`
value — invisible to a grep for an `"image_file"` JSON key. Sibling parser-discards
gotcha: [[lie-layered-image-schema]]. See also [[no-silent-fails]],
[[daz-content-library-root]].
