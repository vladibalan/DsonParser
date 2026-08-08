---
name: parser-faithful-no-cross-section-merge
description: "Parser stays faithful to DSON — never merge/override one section onto another (e.g. scene.animations onto scene.materials); expose each faithfully, consumer decides"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 1e21b26f-1203-4498-98db-2835adfe5a55
---

The parser must report DSON data faithfully and must NOT overwrite, fill, or override one node/section's data with another's — **not even when the target is empty or defaulted**. Concretely: do NOT apply `scene.animations` key-0 keyframes onto `scene.materials` channels. Parse `scene.animations` into its own faithful surface and expose it (new accessors, modeled on the formula surface); the consumer (importer) decides whether/how to merge.

**Why:** core parser philosophy — faithful, permissive, non-interpretive. Consistent with formulas being *stored, not evaluated*, scene-vs-library separation (R6.3), and "don't collapse channels." Cross-section interpretation is the consumer's job, not the parser's. The user raised this 2026-06-08 to correct a consumer-side brief that asked the parser to overwrite material channels from animations.

**How to apply:** when any task says "apply/override/merge X onto Y" inside the parser, treat it as a semantic shift — instead expose X faithfully and flag that the merge belongs to the consumer. This flips such a task from internal-only PATCH (overwrite existing channel) to additive MINOR (new accessors) + a required consumer consumption update. The one sanctioned resolution the parser already does is intra-material `image_url → texture_path` linkage (a reference the channel itself holds), not a cross-section import. Relates to [[dson-animations-keyed-material-overrides]] and [[agnostic-plugin-architecture]].
