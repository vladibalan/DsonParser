# DsonParser - Claude Code entry

The canonical, tool-neutral agent guide is [AGENTS.md](AGENTS.md) - start there; it routes to the
project docs and carries the hard rules every agent binds to.

Claude-specific notes:

- The doc-guard and review-guard hooks (`.claude/hooks/`) fire automatically here on edits, reading
  `Tools/DocForm.config.json` / `Tools/ReviewGuard.config.json`; non-Claude harnesses self-enforce
  instead (AGENTS.md "Enforcement differs by harness").
- Claude sessions carry a per-project auto-memory store OUTSIDE this repo (harness-managed); it
  follows `Docs/MemoryConventions.md`. Bringing the store in-repo is a `Docs/Roadmap.md` backlog
  item; until then the memory-autocommit hook stays unwired.
- Windows PowerShell 5.1 environment hazards (commit-message quoting, BOM-less ASCII `.ps1` files,
  msbuild-via-vswhere) are recorded in `Docs/Reference.md`.
