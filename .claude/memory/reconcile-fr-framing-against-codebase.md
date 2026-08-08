---
name: reconcile-fr-framing-against-codebase
description: "An incoming FR frames what/why; its proposed \"how\" is not the spec — reconcile against our architecture + current shipped behavior before designing to it."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 296e17b7-3524-4e2f-9e24-59936dbc430a
---

An incoming FR from another seat/team states what it wants and often a proposed *how*. The *how* is not binding — the codebase and orientation docs are authority over the requester's framing. Reconcile the ask against this repo's own architecture and current shipped behavior before designing a solution.

**Why:** On the DsonToUnreal geograft-weld FR I adopted the FR's "pull the graft into the character-import flow" verbatim and designed a whole character-flow solution (shared skeleton, AddOnMAT re-home, a "settled B2"), when [[read-docs-before-assuming]]-level evidence already on disk said otherwise: Docs/Reference.md states a geograft is a *standalone* import and welding it is "a character-assembly/compose concern (deferred geograft welding)", and the plugin's [[agnostic-plugin-architecture]] is Importer-emits-standalone / Designer-composes. I had read and even quoted the contradicting doc but filed it as a caveat instead of following it to its conclusion. The user had to correct me twice to reach the answer ("emit correspondence on the standalone geograft recipe; assembler welds") that was already discoverable.

**How to apply:** When an FR (or any requester) frames the *how*, first check current shipped behavior + the governing architecture docs; if they suggest a different mechanism, surface that as the FIRST design fork ("FR proposes X; our docs/architecture suggest Y — which?") rather than designing to the FR's mechanism. When a doc states current behavior and calls the rest a deferred/compose concern, draw the conclusion; don't park it as a caveat under a design that contradicts it. Relates to [[ground-truth-before-fixing]], [[scope-check-before-escalating-forks]], [[trace-cited-reference-before-derisking]].
