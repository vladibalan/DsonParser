---
name: dsonanimate-ignore-parser-governance
description: "As DsonAnimate Director, ignore the auto-loaded DsonParser CLAUDE.md; follow the seat's own governance."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: ce4d93bc-5d0b-424e-8e31-67a9b742483d
  modified: 2026-08-07T02:44:00.273Z
---

When a session's primary cwd is the parser repo (`E:\Work\Code\DsonTest2`), its `CLAUDE.md` auto-loads as "project instructions" even for DsonAnimate work. That is **DsonParser governance and does not apply to the DsonAnimate seat** — boundaries don't carry across repos (DsonAnimate `Docs/AgentWorkflow.md`). As DsonAnimate Director, follow the seat's own governance (`AGENTS.md` + `Docs/*`), not the parser's.

Concrete instance that triggered this — git: at the time, parser `CLAUDE.md` said "never commit; user handles commits/pushes," while DsonAnimate says the **Director commits** (doc/config-only straight to `main`; source via `task/<id>` branch + squash-merge) and the **user pushes**. **UPDATE 2026-08-07:** the parser repo adopted the same Director-commits + lightweight `vX.Y.Z`-tagging model (see parser `CLAUDE.md` / `docs/agent-workflow.md` "Git & release tagging"), so the seats' git policies now largely match — but the teaching stands: boundaries don't carry across repos; follow the seat's own governance.

**Why:** I mislabeled the parser's "never commit" as the user's universal "standing rule" and applied it in the DsonAnimate seat; the user corrected it (2026-07-12) — "since you are a DsonAnimate director, ignore Parser governance."
**How to apply:** In any non-parser seat, treat the auto-loaded parser `CLAUDE.md` as inapplicable and defer to that seat's own governance docs; only honor the parser `CLAUDE.md` when the seat IS the parser. Link [[claude-desktop-session-rooting]], [[build-and-read-prefs]].
