---
name: value-types-are-scaffolding
description: "Unused Dson value types are intentional forward-scaffolding, not dead code — do not remove"
metadata: 
  node_type: memory
  type: project
  originSessionId: 986a3ea8-553e-4998-8077-f8e7d4488ab0
---

In `DsonParser/DsonDataTypes.h/.cpp`, several value-wrapper types are defined but
not yet referenced by the parser or C API: `Bool`, `Float`, `Vector2`, `Color`,
`ChannelType`, `ChannelValue`, `IndexedIntArray`, `IndexedFloatArray`.

These are **intentional scaffolding**, NOT dead code. The project is a DSON (DAZ
Studio asset) parsing library; these types correspond to data shapes that may
appear in DSON/DSF/DUF files not yet covered, and they pre-stage future parser
implementations.

**Why:** User explicitly chose to keep them (2026-06-04) after a DRY/compactness
review flagged them as unused — the domain roadmap justifies their presence.
**How to apply:** Do not propose deleting these as dead code. Compactness/DRY
reviews should leave them in place. See [[build-and-read-prefs]] for the
step-by-step refactor workflow.
