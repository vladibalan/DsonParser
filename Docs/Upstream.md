# Upstream Module - opacity, the formal request, and the pin token (framework core, code layer)

A **verbatim framework core**: domain-free, byte-identical across consumers. Applies to any project that
**vendors or consumes an upstream** it does not own (a pinned library, a sibling repo's published
output). It carries the opacity discipline, the formal-request (FR) channel with its two gate tests, and
the single-canonical-pin-token rule (**COD-10**). A project with no upstream omits this module.
Cold/on-demand. The project's own FR archive and pin record are local bindings, not part of this core.

## Upstream is opaque

Consume only an upstream's **published carriers** - its version, its changelog, its release tag, its
public headers/API - never its private/internal source, **not even to diagnose**. An upstream's
implementation stays opaque: never re-implement it, never cite its internal governance (its rules, its
decision log). A fact findable only in private source is a **finding routed to the user**, never a
design basis. (Consuming the published carriers and the public API is not "citing internals".)

## Reach it only by a formal request, through the user

An upstream is reached **only** through a formal request - via the user, to the upstream's maintainer -
never by editing its files. The FR states **what** (the data and the access need), never **how** (never
the signature or the algorithm: a "how" risks re-specifying what already ships, and a second copy of a
borrowed transform drifts silently - a wrong result does not throw). Route each FR to the maintainer of
the specific upstream it concerns; adoption rides the **version pin** - re-pin to the release that ships
it (COD-10).

### Two tests, before you draft

1. **Is it an upstream ask at all?** A datum already exposed **or derivable** from what ships is **local
   work** - search the published surface first. Read "derivable" strictly (do not offload real work as
   "derivable") but not loosely (a candidate is not the authored answer).
   - **Blind-spot caveat.** This test can never tell you what an upstream has shipped **since** your
     pin: a vendored copy is a frozen tag-snapshot. Reading its changelog/version and concluding "they
     have not published it" is a category error - for the upstream's **current** state there is exactly
     one source, **ask the user**.
2. **Does it stay on their side of the line?** Ask only for **data they authored or read** - never for
   work that is yours to do here (resolving, interpreting, or validating their data onto your model).
   "It is a lot of work" is not a boundary argument. Do not do an upstream's job here; do not push your
   job there.

### The frame: say what must be PUBLISHED

An FR is not a request that may be declined - it is "we need X on the published surface," routed by the
user, and the receiver owns the **how**. State, in order: the **what**; its **shape** (the fields, as
the source states them - unreinterpreted); **which cases it must cover**; **why it is not derivable**
from what ships; and explicitly **what you are NOT asking for**, so the receiver can catch over-reach.
Invite push-back. Cite no governance from either repo - the FR must stand alone in a cold chat.

## COD-10 - Re-pin and pin-token in the same change

A **re-pin** of a vendored upstream and its documentation are **one change**:

- **Bump the single canonical pin token** - the *one* place the pinned-version literal lives. Every
  other mention says "the pinned version" and points there; a duplicated or stale literal is
  stale-orientation drift (the text-layer same-change rule, TXT-7).
- **Record the adoption in the same entry** - what the new version buys, the build result.
- A re-pin alone bumps **nothing** in your own versioning; only surface-affecting consequences of
  adopting it do (COD-11). The upstream contract is its published carriers only (opacity, above).

The re-vendor **mechanics** (fetch/checkout the release tag, rebuild, a compat gate that refuses a
downgrade) are the vendored-upstream layer's ritual (unreal-plugin), not this core - this rule owns the
**obligation**, that ritual owns the **how**.

## The raised archive

The project keeps its own **raised-FR archive** (one line per FR; the *why* in the decision log, status
in the roadmap) and a **pre-identified-not-raised** list for asks it can see coming - never raised
speculatively (the just-in-time discipline: a capability is asked for when a real consumer needs it, the
disciplines core). These are project-owned local docs; this core owns only the channel and the tests.
