---
name: dson-formula-data-external-dsfs
description: "DSON formula data (control dials, ERC follow) lives in external referenced DSFs reached via the scene.modifiers walk; JCMs are the EXCEPTION (auto-followed/eager-loaded, NOT reachable — located by directory-scan/discovery, re-architected 2026-06-15 Artisan-controlled). Not inline in the DUF nor the base figure modifier_library"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 1532f03a-dd54-4f06-9342-80147bd2e905
---

DAZ DSON **formula** data — character/control dials and **ERC rigging-follow** (morph drives a
bone `?center_point`/`?end_point`) — lives in **external referenced DSFs**, reached via the
`scene.modifiers` reachability walk. (**JCMs are the EXCEPTION — NOT reachable**: DAZ auto-follows /
eager-loads them from the figure's `data/.../Morphs/` tree, so a reachability walk never opens them;
locating them is a directory-scan / discovery job, re-architected 2026-06-15 to be Artisan-controlled —
see [[jcm-corrective-import-task]].) It is **NOT**:
- **inline in the imported DUF** — the DUF's `scene.modifiers` entries are bare `#fragment`
  references carrying only the dial value (e.g. `{ "url": "#HID Nancy 9" }`). So calling the
  parser's scene-modifier formula accessors on the **DUF handle returns 0** (the parser is
  faithful and does not resolve cross-file refs — see [[parser-faithful-no-cross-section-merge]]).
- **in the base figure modifier_library** — G9 `Genesis9.dsf` has only ~14 formulas, and they're
  **node-level** (not exposed by the modifier formula API). G9's JCMs/proportion correctives are
  separate external pJCM files, not the base figure.

So an importer/recipe pass that wants formulas must **open the referenced files and walk THEIR
`modifier_library`** via `GetModifierFormula*`. `FDsonMorphBuilder::DiscoverFormulaReachableDocuments`
(DsonToUnreal) does exactly this discovery+open walk (`OutHandles[i]==OutDocs[i]`, base figure
excluded) — reuse it rather than re-deriving.

**Confirmed — DsonToUnreal recipe Slice 5 (2026-06-11):** scope limited to DUF + base figure
emitted `formulas=0` on G9 Nancy; broadening to the transitive external set →
`formulas=2032` (morphval=7 control tree, erc≈2023 proportion-morph bone-follow, other=2 FACS
eye `?rotation`; bound=2024 to imported `UMorphTarget`s; rigpoints=138). The bulk is generic G9
figure-proportion ERC (present for any G9 character) — faithful, consumer-filterable.

**Lesson:** verify DSON structural assumptions against the **actual `.dsf` on disk**
([[daz-content-library-root]] — D:/Daz_content), not abstractly. A pre-code design read that
reasoned about structure without opening the asset assumed JCMs were in the base figure
modifier_library and was confidently wrong; merge-then-measure ([[director-runtime-verify-via-user-import]])
caught it on the import.
