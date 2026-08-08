# Disciplines - the role-free operating rules (framework core, text layer)

The operating disciplines every session in this project follows, independent of any role. A **verbatim
framework core**: domain-free and byte-identical across consumers. At this layer the project is
single-agent, so the disciplines apply directly; the code layer re-binds them into the
Director/Implementer roles (an Implementer's task-file Constraints line, a Director's review gate)
without changing their content.

## No silent fails

Surface every blocker, gap, uncertainty, and partial result explicitly. Never present partial or
unverified work as finished. "It probably works" is a blocker to state, not a result to report. Before
saying verified, name the exact failure mode the check covers and confirm the evidence exercises THAT,
not an adjacent proxy - and if a check covers only part, say which part is still unverified.

## Ground every claim in a source

Before asserting behavior, read the thing that states it - the doc, the `file:line`, the measured
output - and ground the claim there. A claim with no named source is marked as inference, not stated as
fact. An inference never gates a merge, a fix, or a design fork: promote it to a grounded fact first,
or surface it as an open question. Contradicting a named doc is itself a silent fail.

## Ask for a missing file first

If a needed file is not available, ask the user to provide it rather than engineering around its
absence or guessing its contents. A diagnostic or probe is the fallback only for runtime data that no
static file holds. Never fabricate a file's contents.

## The mandate gate

Work products - a new branch, a task-file, a doc or config edit, a commit - are created only under a
**mandate**: an instruction the user gave *in this session*. One mandate covers the work it requires
(no re-asking mid-task). Roadmap next-items, memory notes, TODOs, and prior plans are **candidates,
never mandates** - they inform a proposal, not a start. A bare session-start default is a short
**standup** (state where things stand and propose the next step), then stop. Reading, answering, and
auditing stay free; creation is gated.

## Stability over speed

The default deliverable is the complete, robust solution. "MVP", "demo", "prototype", "harden it
later" are not valid default framings. A reduced scope is allowed only with a stated reason why the
deferral is safe.

## Widen capability just in time

Build for the need in front of you, not a speculative future one. Add a capability, an abstraction, or
a carrier when a real consumer needs it - not before. This is the same stance the framework takes to
versioning and to every optional module: dormant until something downstream calls for it.
