---
name: ps51-git-commit-message-quotes
description: PS 5.1 git commit messages - literal double-quotes break -m, and piping to -F - adds a BOM; write an ASCII temp file and git commit -F it
metadata: 
  node_type: memory
  type: project
  originSessionId: 85c96cad-023c-4a02-b0d6-08be70362a7a
---

In Windows PowerShell 5.1, literal double-quote characters inside a `git commit -m` message break native-exe argument passing to `git.exe` — even when the message is a single-quoted here-string (`@'...'@`). PowerShell mangles the embedded `"`, so git receives the message words as separate args and fails with `error: pathspec '<word>' did not match any file(s)`; the commit does not happen (HEAD unchanged, the file stays staged).

**Why:** PS 5.1 has a long-standing native-argument-quoting defect; an embedded `"` splits the argument when calling a native executable. The here-string parses fine in PowerShell itself — it's the handoff to git.exe that breaks. (Hit 2026-06-10 committing a DsonArtisan doc whose message quoted a section name in double-quotes.)

**How to apply:** Keep commit messages double-quote-free (reword / drop the quotes), or write the message to a file and `git commit -F <file>`. Single quotes/apostrophes are fine; only `"` triggers it. **Encoding trap (hit 2026-06-14):** do NOT pipe the message to `git commit -F -` — PS 5.1 prepends a UTF-8 BOM, so the subject silently starts with an invisible `﻿` (visible as a stray leading char in `git log --oneline`); fix an already-made commit with `git commit --amend`. Robust pattern: `Set-Content -LiteralPath $tmp -Value $msg -Encoding ascii` (ASCII = no BOM) then `git commit -F $tmp`, and **leave the temp file** rather than `Remove-Item` it — the spaced-path guard blocks Remove-Item whenever the command also names the `D:\Unreal` repo path (see [[glob-spaced-path-plugin]]). **Put `$tmp` in the SESSION SCRATCHPAD, never inside the repo** — past sessions wrote them to `.handoff/`, and ~26 stale `commitmsg-*.txt` accumulated there until the 2026-07-08 housekeeping sweep archived them to `.handoff/history/`. Also remember `git commit -F` only commits **staged** changes — `git add` first. Sibling PS 5.1 gotcha: [[ps51-hooks-ascii-only]].
