# Native-binding module (framework core, cpp layer) - CPP-3, CPP-4

A **verbatim framework core**, and an **optional** one: a project includes it only where it exposes or consumes
a **native C ABI** (a DLL / shared-library boundary with C linkage) or owns an external resource through a raw
handle. Where the project has no such boundary, both rules are **n/a-stubs** - keep the number, state n/a
(DecisionLog D8, the retired/n/a convention). Domain-free (abstract C++ illustration, D9); the concrete export
list, sentinel values, and wrapper type are a project's own - they live in its source and its cpp Reference
starter, never inline here. These two rules join the assembled rulebook after CPP-1..2 when the module is
adopted.

## CPP-3 - A native binding has one source of truth

A C-ABI boundary is a **published contract** (COD-4): the exported surface and the client's binding to it must
have exactly one edit site, so a change cannot half-land - a new export the client never binds, or a signature
the two sides disagree on. Two proven patterns; use whichever fits:

- **The single export header.** One `extern "C"` header is the whole surface - every exported function, an
  opaque handle type, the import/export decoration - and nothing is exported that is not declared there. A
  consumer binds against this header alone.
- **The X-macro binding list.** One list macro enumerates the surface as rows (required, return type, name,
  parameters); the same list generates the client-side function-pointer struct, its `IsValid()` completeness
  check, and the runtime bind loop that resolves each symbol from the loaded library. Add / change / remove an
  export in exactly one place - the row - never a parallel typedef / struct / load list; a compile-time
  assertion that the generated struct matches the header keeps the two sides honest.

**The per-family return-value contract.** A flat C ABI encodes success / failure in the return value, and the
encoding **differs by function family** - the same value means opposite things across families: a status where
`0` = success; a count where `0` = the failure / empty sentinel; an index accessor where a negative value =
"no such element"; a pointer where null = failure; a bool; a string where empty = missing. This is the single
biggest source of consumer bugs. State the per-family contract once, at the header (COD-4), keep it accurate
when a return convention changes, and in review confirm which family a changed function belongs to before
judging its sentinel. (A project records its own family table as a cpp Reference seed.)

## CPP-4 - External resources are RAII; borrowed pointers are copied at once

- **One RAII wrapper per external resource.** A raw handle from a native API - a document / context / connection
  acquired by a create / open call and released by a destroy / close call - is owned by a single RAII wrapper:
  acquire in its constructor or a factory, release in its destructor, non-copyable (move-only if it must
  transfer). No hand-rolled create / destroy at call sites - that reintroduces the leak-on-early-return the
  wrapper exists to remove. (n/a where the project owns no external handle.)
- **Copy a borrowed pointer before the next call.** A `const char*` (or any pointer) returned by a native API is
  **transient** - it points into storage the next call across that boundary may invalidate or reuse. Copy it
  into an owned value immediately, before the next call into that library, and flag any code that holds a
  borrowed pointer across another call into the same boundary.
