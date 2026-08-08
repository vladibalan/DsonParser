---
name: parser-changelog-authority
description: Vendored parser CHANGELOG is the authority for DsonParser C-ABI behavior from the plugin seat; read it before FR-ing/speculating. current_value already preferred for material/scene/modifier channel value reads.
metadata: 
  node_type: memory
  type: reference
  originSessionId: 019f31f1-7966-4c07-989d-c3bde1cfff93
---

From the DsonToUnreal plugin seat, the vendored parser CHANGELOG at
`Source/ThirdParty/DsonParser/Include/CHANGELOG.md` (ships beside the header/DLL) is the authority for
how the DsonParser C-ABI actually behaves — **read it before proposing a parser FR or speculating
about parser behavior.** (2026-07-02: the user had to redirect me here after I speculated that
material-channel value reads might return `value` not `current_value`.) [[read-docs-before-assuming]]

Settled facts as of vendored **2.6.0**:
- **Numeric channel value reads already prefer `current_value` over `value`** for material,
  scene-material, and modifier channels — that was the baseline; **2.4.0** only brought *transform*
  channels in line ("matching other scene-channel reads"; ParseTransformVector3 prefers current_value
  for scene-node + library-node reads = the Jewel Bikini / Gem Drop placement fix, bug 3).
- **2.2.1**: bool channel values coerce to 1.0/0.0 in `GetMaterialChannelValue` /
  `GetSceneMaterialChannelValue` (+ modifier equivalents) = the G8/G8.1 "JCMs On" gate fix.

So reading a material_library float channel (e.g. HipWrap `Cutout Opacity` `current_value:0.2`, `value:1`)
returns **0.2**, not the `value` default → the wrap "still white" bug is NOT a parser gap; no FR needed.
See [[multipart-wearable-b2-status]].
