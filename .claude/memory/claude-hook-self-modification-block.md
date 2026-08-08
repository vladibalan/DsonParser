---
name: claude-hook-self-modification-block
description: "Editing my own .claude/hooks/*.ps1 triggers an auto-mode self-modification block; get explicit user auth, never bypass."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: e1fa25bf-ea8f-4c12-b02f-f257d4e676c8
---

Editing a `.claude/hooks/*.ps1` guard script (e.g. retuning `doc-guard.ps1` budgets/rule-counts or `review-guard.ps1`) is denied by the auto-mode classifier as "self-modification of a behavioral guard hook" — even when the user broadly approved the task ("update the docs", "do it"). Hit 2026-06-25 fixing a stale `R1-R11`→`R1-R12` reminder string.

**Why:** the harness treats an agent editing its own behavioral guards as a higher-bar category than the doc/config edit it looks like; a general task approval doesn't cover it.

**How to apply:** surface the exact diff + which hook file, ask the user for explicit authorization for *that* edit, then retry the same Edit (it goes through). Never route around the denial with `Set-Content`/`Out-File`/`sed` — that defeats its intent. Keep hook .ps1 edits pure ASCII ([[ps51-hooks-ascii-only]]). In these repos the active hooks are user-global but point at the repo's `.claude/hooks/` by absolute path, so there's one script to edit ([[claude-desktop-session-rooting]]).

**Refinement (2026-07-11):** a batch approval doesn't count — the user's "approved, proceed" over an audit-findings list that explicitly named the doc-guard fix (`R1-R12`→`R1-R13`, DsonArtisan) was still denied. The authorization that unblocks is the one given *after* the denial, for that single edit, in its own message. Commit the rest of the change without the hook file and leave the hook line as a named pending item.

**Refinement 2 (2026-07-11, DsonAnimate standup):** the block also covers user-global `~/.claude/settings.json` (adding hook *entries*, not just editing scripts), and there pre-authorization did NOT unblock a retry. Proven fallback: back the live file up to scratchpad, Write the complete prepared file to scratchpad, prove it (ConvertFrom-Json parse + `git diff --no-index` vs backup showing additions-only), hand the user a one-line `Copy-Item` to apply, then verify by SHA256 hash-match against the prepared file. New hook entries only take effect for new sessions.
