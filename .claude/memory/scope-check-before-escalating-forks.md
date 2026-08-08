---
name: scope-check-before-escalating-forks
description: "Before raising an upstream-change or decision fork, confirm the simple in-scope path doesn't already deliver the user's goal"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 7fdca49a-cc80-4dc9-80ef-ed7daff660eb
---

On the DsonArtisan pre-2.1b base-corrective task I presented the user an M1-vs-M2 fork
(Artisan-side carry-via-`DuplicateAsset` vs. an upstream importer-API change) and asked them to
choose — when M1 already delivered the goal (a self-contained derived mesh) with **no** importer
change. I had inflated a secondary "keep the imported intermediate mesh lean" disk-economy aside
into a blocking fork, even though the governing decision itself said storage was modest and
faithfulness outranked it. The user pulled me back: "I do not understand why the Artisan cannot
deliver my request ... bring a director back to the scope."

**Why:** manufacturing forks/blockers from non-binding constraints wastes round-trips and
over-uses the decision tool, and the user has repeatedly had to pull directors back to scope. This
is distinct from surfacing *real* gaps — surfacing the genuine guard edge-case later in the same
task was correct and welcomed.

**How to apply:** before raising an upstream-change fork or firing AskUserQuestion, first confirm
whether the simplest in-scope (Artisan-side) mechanism already delivers the user's stated goal. If
it does, just propose that and proceed. Reserve forks for genuine either/or decisions where the
simple path truly cannot deliver. Keep surfacing real blockers/gaps/edge-cases [[no-silent-fails]];
the point is not to invent forks from secondary niceties. Related: [[decisions-grounded-in-principles]],
[[parser-changes-route-through-user]].
