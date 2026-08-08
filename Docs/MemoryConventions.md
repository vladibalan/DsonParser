# Memory Conventions - the cross-session store (framework core, text layer)

A **verbatim framework core**: domain-free, byte-identical across consumers. Conventions for a
per-project memory store that carries working context across sessions. The store's location is
harness-specific (a local binding names it); the conventions here are portable.

## File per fact

Each memory is one file holding one fact, with a small frontmatter block - a name, a one-line
description used to judge relevance at recall, and a type. One index file lists every memory as a
single pointer line and is the only memory loaded up front. The body links related memories by name.

## What belongs in memory vs docs

- **Docs are the record of the project.** Durable facts, decisions, and rationale live in the project
  docs (durable facts -> the Reference tier; dated rationale -> the DecisionLog tier). Memory never
  duplicates what a doc already owns.
- **Memory holds cross-session working context** the repo does not record: who the user is, how they
  want the work done, the shape of an in-flight goal. When an arc closes, its durable residue offloads
  to the DecisionLog and the working notes are pruned.

## Prune taxonomy

On a reflective pass, each memory is one of: **keep** (still true and useful), **consolidate** (merge
duplicates into one file), **offload** (a closed arc whose residue moves to a doc), or **fix-stale**
(the fact changed - correct or delete it). A memory records what was true when written; verify a named
file or flag still exists before acting on it.

## Config self-modification guard

Changing the harness's own configuration or settings (hooks, permissions, standing rules) needs an
explicit user instruction in the session - never a silent side effect of other work. This is the
mandate gate applied to the project's own machinery.

## Autocommit (optional)

A memory-autocommit hook may commit the store on session end so notes are never lost. It is an optional
local convenience, not part of any verbatim core's behavior. The layer ships one
(`hooks/memory-autocommit.ps1`, a Stop-event hook committing only the store's pathspec); adopt or
ignore it.
