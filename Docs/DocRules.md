# Doc Rules - the doc economy (framework core, text layer)

The LLM-friendly documentation rules every doc in this project binds to. A **verbatim framework
core**: it ships byte-identical to every consumer and carries no project-specific content - all local
values (the path marker, the per-doc budget table, the sealed dirs) live in
`Tools/DocForm.config.json`, which the doc-guard hook and the doc-form census both read. Improve a
rule here once and every consumer gets it at the next sync. These rules bind **every** `.md` in the
repo, product files included.

## Why

Agent tooling is line-oriented and context-budgeted: Read truncates long lines, grep finds headings,
and every hot-path byte taxes every future session. An LLM pays per token (~4 bytes), so budgets bind
in lines AND kilobytes - a line budget alone is defeated by line width.

## Tiering

- **One tier per doc; point, don't duplicate.** If another doc owns a fact, link it by name - never
  restate it. Every fact has exactly one home.
- **Status holds current state only.** History (superseded plans, settled-fork rationale beyond the
  decision entry) migrates to cold docs rather than accreting in hot ones.
- **The doc-type tiers** (a consumer materializes these as real files):
  - Entry / session pickup -> `AGENTS.md` (canonical, tool-neutral) + `CLAUDE.md` (Claude adapter)
  - Forward direction, and why -> `Intent`
  - Open status, what is next -> `Roadmap`
  - Settled dated rationale -> `DecisionLog`
  - Durable facts / lessons / gotchas -> `Reference`
  - Doc form + tiering rules -> this doc
  - The role-free operating disciplines -> the disciplines doc

## Same-change sync

- **Docs ride the change, never a follow-up.** Stale orientation misroutes the next session, so when
  a change alters layout, routing, or a stated fact, its doc update lands in the SAME change - a wrong
  doc is worse than no doc.
- **Status rides the change too.** The open-status doc and any settled decision move to current in
  the same change that ships the work, derived from that change's own diff, not reconstructed later.

## Form (every doc, hot or cold)

- **Hard-wrap prose at ~120 chars.** Over the signal cap (framework default 400) on one line is a
  defect signal; **the hard cap (default 2,000) has no stated-reason escape** - agent Read truncates
  there, the overflow is invisible, and the doc-guard hook blocks the save.
- **Tables carry short enumerable facts only.** Paragraph-scale content lives under a grep-able
  heading, never in a table cell.
- **ASCII only.** A cp1252 shell round-trips mojibake for non-ASCII bytes (em-dashes are the classic
  casualty); the census warns on any non-ASCII char.
- **Log-shaped content gets dated, grep-able headings** so entries are found by grep, not by reading
  the file top to bottom.
- **Point to a greppable anchor, never a `.md:line`** - line numbers drift; a heading or id does not.

## Budgets

- **Every doc - one universal ceiling** (framework default `<=1,000 lines AND <=64 KB`), binding
  every doc, cold ones included; warn-only (only the hard line cap blocks). Crossing it is the signal
  to shrink by shape, never to keep growing: a **dated append-only log** rotates its oldest entries
  verbatim into a sealed volume (`Docs/<Name>/<range>.md`, immutable once sealed) with a per-entry
  index kept in the root; a **living fact store** topic-splits into `Docs/<Name>/<Topic>.md` files
  with the root as pure index. Cite through the root (by date or title), never by volume file.
- **Hot-path docs carry tighter soft budgets, lines AND KB** - the earlier relocate-or-split signal,
  well inside the universal ceiling. The per-doc table is project-specific and lives in
  `Tools/DocForm.config.json`, not here.
- **Retune values only under a recorded decision** - in one change, in the config both the hook and
  census read (a single source, so they cannot disagree).

## Cold-doc rotation, in detail

When a cold doc crosses the ceiling, rotate by its shape - a doc-only change, moved verbatim
(per-entry conservation), wrapping any long line during the move:

- **Dated append-only log** (entries never edited after writing): move the oldest entries into a
  sealed volume `Docs/<Name>/<range>.md` - immutable once sealed, and never split one entry across
  volumes. The root keeps the newest entries plus a per-entry index line at the bottom:
  `YYYY-MM-DD - <title> -> <volume>`. Cite by root + date, never by volume file.
- **Living fact store** (titled facts updated in place): move entries into topic files
  `Docs/<Name>/<Topic>.md` - not sealed, edit freely; the root becomes a pure index
  (`<title> -> <file>`). Adding a fact is a topic file + an index line in the same change. Cite by
  root + title, never by topic file.

## Enforcement

- **The doc-guard hook** (`.claude/hooks/doc-guard.ps1`) fires on Edit/Write to any repo `.md`:
  PreToolUse injects the tier + form reminder; PostToolUse warns on budget/form breaches and
  **blocks** a save that crosses the hard line cap. It reads `Tools/DocForm.config.json`.
- **The doc-form census** (`Tools/Check-DocForm.ps1`) is the repo-wide backstop for writes the hook
  cannot see (bulk scripts, human editors). Run it before committing a doc-heavy change; `-Strict`
  promotes warnings to failures (the publish-gate mode). It reads the same config.
- **Self-audit is the primary gate.** After editing any doc, state the form/tier check result naming
  what you checked - never "looks fine". The hook and census are backstops, not the gate.

## Enforcement differs by harness

The hook fires automatically only inside a harness that runs it (e.g. Claude Code). Other agents get
no hook backstop: self-audit against these rules and run the census after doc work. A per-harness
hook mirror is a downstream option, not a guarantee.
