# DsonParser - Intent

Forward **direction**: where the work is headed and why, *before* that direction hardens into
status (`Docs/Roadmap.md`) or shipped rationale (`Docs/DecisionLog.md`). Subordinate to
`Docs/Principles.md` - intent never overrides a principle. Approved by the owner 2026-08-08.

## North star

Complete DSON asset fidelity through the C ABI: everything a DAZ file states, faithfully exposed,
so any engine importer can rebuild the asset - geometry, materials, rigging, morphs, dials -
without DAZ-side tooling. The current epoch is the v2 formula work (control dials / ERC / JCM data
exposure) per `DsonParser_Roadmap.md`. Out of scope: evaluating or interpreting that data (the
consumer's job, P1), rendering semantics, and DAZ-side tooling.

## Tensions to honor (not yet scheduled)

- Accessor fan-out vs model growth - the flat C ABI's per-field pattern must not outgrow the model
  it exposes; the tripwire audit (`Docs/AuditGuide.md`) watches the trend, and the bounded remedy
  is a struct-returning ABI for leaf-heavy sections only.
- Permissiveness vs silent data loss - P2's tolerance must never hide dropped data; the unknown-key
  audit is the counterweight.
- Faithful exposure vs caller ergonomics - bulk/struct accessors only when ergonomics genuinely
  hurt, never as interpretation.
- ABI stability vs capability growth - widen additively (MINOR); breaking shapes only on the
  user's explicit call.
