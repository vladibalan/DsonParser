# DsonParser - Claude Code entry

The canonical, tool-neutral agent guide is [AGENTS.md](AGENTS.md) - start there; it routes to the
project docs and carries the hard rules every agent binds to.

Claude-specific notes:

- The doc-guard and review-guard hooks (`.claude/hooks/`) fire automatically here on edits, reading
  `Tools/DocForm.config.json` / `Tools/ReviewGuard.config.json`; non-Claude harnesses self-enforce
  instead (AGENTS.md "Enforcement differs by harness").
- Claude sessions carry a per-project auto-memory store, now IN-REPO at `.claude/memory/`
  (relocated from the external harness path via the `autoMemoryDirectory` binding in
  `.claude/settings.local.json`, a per-seat local binding); it follows `Docs/MemoryConventions.md`.
  The `memory-autocommit` Stop-hook (`.claude/hooks/memory-autocommit.ps1`) commits the store on
  session end.
- Windows PowerShell 5.1 environment hazards (commit-message quoting, BOM-less ASCII `.ps1` files,
  msbuild-via-vswhere) are recorded in `Docs/Reference.md`.
