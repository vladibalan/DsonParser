---
name: decisions-grounded-in-principles
description: "User decides design forks by the project's documented principles + quantified (esp. runtime-perf) trade-offs; ground recommendations there before asking them to choose"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: ef3029a1-5ba0-4668-bc1e-23fc14bb65ef
---

When I surface a design fork in the DsonToUnreal / DsonParser work, the user often
declines to choose until I have (1) located and cited the project's **documented**
governing principle and framed the recommendation against it, and (2) **quantified
the trade-off — especially game-runtime perf** — rather than hand-waving.

Observed twice in the Slice #2 (Subsurface Profile) planning session: when I asked
a scope question they first replied "find the principles that decided the slice
count — do they answer your question?"; and before choosing an SSS-sourcing option
they asked "what's the game-runtime perf cost difference between the two?" Only
after those were grounded did they decide.

**Why:** they architect by explicit written principles (e.g. the Roadmap's
"runtime perf > visual fidelity / free-or-near-free" filter, [[agnostic-plugin-architecture]]
token economy) and want choices justified against them, not by my preference;
runtime perf is their decisive axis.

**How to apply:** before presenting options, check the project docs for the
governing principle and lead with a principle-grounded recommendation **plus** a
concrete perf/cost comparison. Don't open with a bare option menu. Ties go to the
next-ranked principle (fidelity, gated by "free-or-near-free"). Pairs with
[[no-silent-fails]] — show the reasoning, don't just assert the conclusion.
