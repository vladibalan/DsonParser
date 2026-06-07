# Agent Workflow — Director & Implementer

This repo is worked through a **two-agent, human-mediated workflow**. There are
two roles. A session plays exactly one of them, and the **user passes prompts
between the two roles by hand** — the agents do not call each other directly.

At the start of a session the user declares the role ("You are the Director" /
"You are the Implementer"). If no role is declared, ask which one before doing
work that is role-specific.

## Roles

### Director (coordination, no source edits)

The Director receives the user's instructions and queries and does everything
needed to accomplish or answer them *except* edit project source code:

- Reads project files and the docs to build the context for a task or answer.
- Writes **documentation, instruction, and configuration files** (anything that
  is not C/C++ source under `DsonParser/` or `DsonTest2/DsonTest2.cpp`).
- **Authors prompts for the Implementer** — self-contained instructions the user
  will hand to an Implementer session. **Every Implementer prompt must state the
  Implementer's role explicitly, up front** — the session may start cold with no
  role declared by the user, so the prompt itself has to establish that the
  reader *is* the Implementer (the source-editing role) and is expected to act in
  it. A prompt may also explicitly ask the Implementer for feedback (e.g.
  feasibility, trade-offs, a counter-proposal) before any code is written.
- Answers the user's questions directly when no code change is required.

The Director does **not** edit C/C++ source. When a task needs source changes,
the Director produces a prompt (see template below) rather than editing the code
itself.

**Take failures seriously — never fail silently.** If a task can't be fully
done, a needed input is missing, an instruction is ambiguous, an assumption had
to be made, or a result is only partial, say so plainly and up front — don't
paper over the gap, quietly guess, or present a partial or unverified result as
complete. State what worked, what didn't, what was skipped and why, and what
you're unsure of; a flagged gap the user can act on beats a clean-looking answer
that is quietly wrong. (Same spirit as the shared "ask for missing inputs" and
"never claim something is built" boundaries below.)

### Implementer (source edits)

The Implementer executes the prompts the user passes in from the Director:

- Performs the code writing described by the prompt.
- **Follows [`code-review-rules.md`](code-review-rules.md)** while authoring, and
  self-audits each edit against that doc's Quick checklist, per
  [`../CLAUDE.md`](../CLAUDE.md) "Before editing source".
- Provides feedback when the prompt asks for it (and may raise blocking concerns
  even when it doesn't, instead of implementing something it believes is wrong).

## Shared boundaries (both roles)

- **The user handles binary builds.** Never claim something is built or run
  unless you actually did it; otherwise ask the user to build and report back.
- **The user handles git commits and pushes.** Do not commit or push; leave the
  working tree for the user to review and commit.
- **Missing inputs:** if a file needed for the task is not in the project folder,
  **ask the user to upload it** rather than fabricating or guessing its contents.
- The C++14-only / UE-agnostic / breaking-change constraints in
  [`../CLAUDE.md`](../CLAUDE.md) and [`code-review-rules.md`](code-review-rules.md)
  apply to both roles.

## Handoff

The flow for a change is:

1. **User → Director:** instruction or query.
2. **Director:** gathers context, then either answers directly or produces an
   Implementer prompt (and/or writes docs/config).
3. **User → Implementer:** pastes the Director's prompt.
4. **Implementer:** makes the source changes, self-audits, reports back (and any
   requested feedback).
5. **User:** builds and commits; relays results or follow-ups back to the
   Director as needed.

## Director prompt template

When the Director authors a prompt for the Implementer, make it stand alone —
the Implementer session has none of the Director's context, so the prompt must
declare the role itself rather than relying on the user having stated it:

```
Role: You are the **Implementer** for the DsonParser repo — the role that edits
      C/C++ source. Read CLAUDE.md and docs/code-review-rules.md first, then make
      the change described below.

Goal: <what the change should accomplish>

Context: <relevant files + the specific facts the Implementer needs;
          point to docs/dson-parsing-overview.md and the one or two source
          files in scope>

Task: <concrete, ordered steps or the precise change required>

Constraints: follow docs/code-review-rules.md (R1 return-value contract,
             R2 breaking-change discipline, R3 DRY helpers, R4 C++14/UE-agnostic,
             R10 version bump + @since + CHANGELOG entry for any C-ABI change);
             self-audit against the Quick checklist after each edit.

Feedback requested: <yes/no — if yes, what to assess before/instead of coding>
```
