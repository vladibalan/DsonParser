---
name: trace-cited-reference-before-derisking
description: "Before claiming a change \"mirrors a proven path\"/\"is de-risked\", trace that reference's exact logic line-by-line; gesturing at it caused two regressions in the DsonArtisan JCM 2.1c arc."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 9f582c8f-c067-4694-baff-2406c1e0df98
---

When justifying a design or fix by saying it "mirrors a proven path" or is "de-risked" by an existing implementation, open that reference and trace its exact logic — key handling, ordering, precedence, edge cases — before asserting parity, in the task-file spec **and** again in review.

**Why:** In the DsonArtisan JCM driving 2.1c arc (2026-06-19), two regressions both came from asserting de-risking without tracing the cited reference. (1) Shape-clobber: framed driving static-gated value-chained entries as a harmless "over-build" — it drove the character's shape morphs to 0 and collapsed the figure to base. (2) Dial-seeding inert: claimed it "mirrors EvaluateFormulaGraph" but missed that the graph strips the `?value` query on the lookup (`StripQuery(CanonUrlKey(...))`), so the dial lookup never matched and shipped doing nothing (`dialResolved=0`). Built-in diagnostics surfaced both — but only post-merge, after a round-trip.

**How to apply:** When citing an existing "proven" function as the reason a change is safe, confirm the specific mechanism you're copying actually transfers (e.g. it uses `StripQuery` on the lookup, not just `CanonUrlKey`; it checks pose-dependence, not just "value-chained"). A diagnostic that reveals failure post-merge is not a substitute for tracing the reference pre-merge. Relates to [[ground-truth-before-fixing]] and [[verify-the-actual-failure-mode]].

**Third instance (2026-07-09, DsonToUnreal Director):** a task-file's Context section asserted `GetUVSetId` was "already bound" in the importer's X-macro table — inferred from the accessor existing in the vendored parser *header*, not from tracing `DsonParserFunctions.h` itself. The Implementer caught it and added the missing row. The same discipline applies to task-file Context claims: any "already exists/already bound/already handled" statement must be traced in the exact file that owns it (declared-in-header ≠ bound-in-table).
