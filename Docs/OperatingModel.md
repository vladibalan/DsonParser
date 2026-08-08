# Operating Model - how a session works here (framework core, text layer)

A **verbatim framework core**: domain-free, byte-identical across consumers. Defines the text layer's
single-agent operating model and the two patterns every consumer inherits with it. The disciplines a
session runs on live in the disciplines core; this doc is the shape of a session, not the rules it
follows.

## Single agent

At the text layer there are no roles: one agent, instructed by the user, works the current task. No
Director/Implementer split, no file-based handoff, no task or feedback files. The operating disciplines
apply directly to that one agent. (The code layer introduces the two-agent split - it pays only once
an objective oracle like a compiler or a test suite absorbs the delegation's iteration cost. Prose has
no such oracle: the reviewer is the same expensive judge and a weak-safe task-file is most of the
draft, so the split does not pay at this layer.)

## Review as a cold pass, not a role

Independent second-eyes still has value on prose: a finished doc is re-read cold - against the rules,
for tier leaks and stale facts - as a distinct pass, not as a separate role or agent. Self-audit is the
author's gate during the work; the cold pass is the check after it. The code layer promotes this same
idea into a formal review gate at the merge.

## Dual entry guide

The project is entered through two files so it is workable by any agent, not only one harness:

- **`AGENTS.md`** - the canonical, tool-neutral agent guide. Every agent starts here. It carries the
  session pickup, the hard rules, and the routing to the working docs.
- **`CLAUDE.md`** - the Claude Code adapter. It defers to AGENTS.md and adds only Claude-specific
  notes (the hooks that fire here, the memory store).

**Enforcement differs by harness.** Hooks fire automatically only inside a harness that runs them
(e.g. Claude Code). Other agents get no hook backstop and must self-audit against the rules and run the
census after doc work. AGENTS.md states this asymmetry so no agent assumes a safety net it does not
have.
