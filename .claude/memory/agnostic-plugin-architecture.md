---
name: agnostic-plugin-architecture
description: User's DAZ→UE pipeline is split into Parser → Importer → Designer plugins for LLM token economy. Reversed 2026-06-09 — Importer now brings everything and emits all authoring metadata faithfully/mechanically (consumer-blind); Designer is pure-UE and never calls the Parser; agnosticism is kept via consumer-agnostic Importer docs + a fence, not by minimizing emission. Designer layer named DsonArtisan (role term "Artisan") on 2026-06-09. Refined 2026-06-10 (DsonArtisan P1): Designer may also *invoke* the Importer's public UE API to trigger imports (first feature = import launcher), still never the Parser.
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 65ddd9c5-2c68-4120-92e9-8af8ae2b5385
---

The DAZ→UE pipeline is split into separate plugins — **Parser → Importer →
Designer** — and the split's purpose is unchanged: **LLM-agent token economy**
(smaller agnostic modules = smaller per-task contexts than a monolith would have).

- **Parser** (`DsonParser`, flat C ABI) — knows nothing about Importer. *(unchanged)*
- **Importer** (`DsonToUnreal`) — consumes Parser; stays agnostic to its consumer.
- **Designer** (future) — a DAZ-Studio-like character *baking*/authoring tool.
  **Named "DsonArtisan"** (role term "Artisan"; chosen 2026-06-09): follows the
  upstream `Dson`+role convention (cf. `DsonParser`), and "Artisan" deliberately
  breaks the *mechanical* Parser/Importer connotation to mark this as the sole
  authoring-intelligence layer. Module/plugin = `DsonArtisan`; in prose = "the
  Artisan" (parallel to Importer = `DsonToUnreal`). Docs/_OutOfScope still say
  "Designer" — migrate that term when doc work is greenlit.

## Reversed 2026-06-09 — how the layers couple

The earlier stance ("minimize emission; the consumer discovers by convention or
re-parses the Parser itself") was **reversed**. Governing direction now lives in the
Importer's `Docs/Principles.md` (P1–P5):

- **Importer brings everything; translates, doesn't interpret.** It emits *all*
  authoring metadata with no UE-asset home (LIE compositing, makeup, JCM rigging
  formulas) as a faithful, **mechanical, consumer-blind** artifact — because bringing
  everything is its job, *not* "for the Designer." Composition / baking / assembly are
  interpretation, out of Importer scope.
- **Designer is pure-UE and never calls the Parser** — it reads Importer outputs only
  (assets by path/id convention + the emitted artifact). This reverses the old
  "Designer re-parses DAZ via the Parser directly."
- **Refined 2026-06-10 (`DsonArtisan` P1):** Designer may also ***invoke*** the Importer's
  public UE API (`FDsonImporterModule::ImportDazAsset`) to trigger an import in-editor —
  delegation, **still never** parsing DSON/DSF/DUF or linking the Parser (it stays
  encapsulated inside the Importer). First DsonArtisan feature = a Tools-menu **import
  launcher** (shipped 2026-06-10, `main` `0991cfe`). Sanctioned via P1 clause + DsonArtisan
  `Docs/DecisionLog.md` 2026-06-10; don't re-flag a Designer→Importer call as a P1 breach.
- **Agnosticism is kept by the Importer not *naming* the consumer, not by emitting
  less.** Importer docs are consumer-agnostic (`Docs/Principles.md`); cross-repo /
  Designer intent is fenced at `Docs/_OutOfScope/` (excluded from Importer discovery).
  So token economy comes from consumer-blind docs + the fence + a mechanical emit that
  doesn't grow per-feature.

## Apply

- Importer emits faithfully + mechanically (one generic projector, isolated); never
  consumer-specific. All interpretation lives in the consumer.
- Completeness is bounded by Parser exposure → widen it **additively, just-in-time**
  when a concrete need lands.
- **Discovery is bounded by the imported asset's reference graph** (deps / companions /
  transitively-referenced files), NOT the wider content library. "Brings everything" =
  everything *in the graph*, faithfully — never "scan the library." Unreferenced sibling
  **authoring** presets — alternate eye-color / lip / makeup options + the LIE textures
  they carry — are out of scope (the Artisan's job). Decided 2026-06-09 via the Nancy-eyes
  question: `HID Nancy 9.duf` references no eye color, so eyes stay **generic by design**;
  pursuing per-character authored eyes/lips/makeup is the authoring layer, not the Importer.
- **Speculative (validate against real DSON at implementation):** the artifact's
  schema, and whether it stays *pure data* (a convention-located file + format spec,
  **no** shared UObject module) or — if it can't — needs a small shared typed module.
- Keep the consumer's *name* out of **forward-looking** Importer docs (Principles /
  CodeReviewRules / Versioning / Roadmap) — refer to a generic "downstream consumer"
  (the project knows it's upstream of *others*, just not dedicated to one); cross-repo
  intent goes behind the `Docs/_OutOfScope/` fence. **Carve-out:** `Docs/DecisionLog.md`
  (dated postmortems / decision records) MAY name the actual consumer factually — it's
  history, not dedication. Enforced by the 2026-06-10 audit that stripped leaked
  `DsonArtisan` names from R12 / Versioning / Roadmap (commit `5b56752`); Principles.md
  was already clean. Codified 2026-06-10 as a **CodeReviewRules R10 sub-point** + Quick
  Checklist clause (commit `a341255`): forward-looking docs say a generic *downstream
  consumer*; a downstream Director's *request* may name its own plugin, but the
  resolution landing in the docs stays generic; only DecisionLog may name *which*. The
  rule's own text names no consumer; R10 is now at its 265-line ceiling (next rule
  needs a relocate/split).

Pointers: `Docs/Principles.md` (governing), `Docs/DecisionLog.md` (2026-06-09 entry),
`Docs/_OutOfScope/README.md` (fenced rationale + speculative specifics).
Related: [[ue-consumer-cpp14-constraint]] (Parser stays UE-agnostic — same principle one
layer up, still unchanged).
