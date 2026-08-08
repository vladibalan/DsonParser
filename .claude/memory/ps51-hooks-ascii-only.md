---
name: ps51-hooks-ascii-only
description: Keep .claude hook .ps1 files pure ASCII — a BOM-less UTF-8 hook breaks under PowerShell 5.1 ANSI decoding and fails on every Edit/Write
metadata: 
  node_type: memory
  type: project
  originSessionId: d2bd1834-8778-4f26-8e97-c96b59368b01
---

DsonToUnreal's `.claude/hooks/*.ps1` run under **Windows PowerShell 5.1**, which decodes a **BOM-less** `.ps1` as the ANSI code page (Windows-1252), *not* UTF-8. A non-ASCII char then corrupts tokenizing: an em-dash `—` (UTF-8 `E2 80 94`) decodes so its `0x94` byte becomes a curly quote `”` (U+201D), which PS treats as a string terminator → **hard parse error**. Because it's a parse error, the *entire* hook fails to load on **every** Edit/Write/MultiEdit it fires on — so feedback-files and doc edits silently never land, looking unrelated to one stray character. Console symptom: `Failed with non-blocking status code: At ...\<hook>.ps1:LINE char:COL`.

**Why:** the parse failure blocks the whole script before its self-filter runs, so the blast radius is every wired tool call, not just the targeted doc.
**How to apply:** keep hook `.ps1` files pure ASCII — use `->` / `-`, never Unicode arrows or em/en-dashes; if non-ASCII is unavoidable, save with a UTF-8 BOM. Verify with `[System.Management.Automation.Language.Parser]::ParseFile($f,[ref]$t,[ref]$e)` under PS 5.1 (0 errors) and scan bytes `>127`. Canonical in-repo home for this lesson is the plugin's `Docs/Reference.md`. Root-caused 2026-06-09 in `dson-doc-guard.ps1` line 56. Related: [[claude-desktop-session-rooting]], [[glob-spaced-path-plugin]].
