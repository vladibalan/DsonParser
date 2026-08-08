---
name: claude-desktop-session-rooting
description: "User works via the Claude Desktop app (Windows); Ctrl+N picks a project folder so each session roots at its own git repo. As of 2026-07-12 each of the 4 seats has its own per-repo memory store; universal disciplines live in user-global ~/.claude/CLAUDE.md"
metadata: 
  node_type: memory
  type: user
  originSessionId: 46847640-160c-4575-9ade-71b342c0c0b9
---

The user's **primary** interface is the **Claude Desktop app** (not the CLI) on Windows; occasionally JetBrains Rider.

**Rooting (updated 2026-07-12).** The Desktop app **can** choose the session's project folder — **`Ctrl+N`** (new session) → select the folder/repo. An older Desktop build lacked the picker and always opened at **DsonParser** (`E:\Work\Code\DsonTest2`), which is how all four seats came to share one memory store; **updating the app restored the picker**. So each seat is now rooted at its **own git repo**, and Claude Code keys memory, `settings.json`, and `CLAUDE.md` from that root.

**Settings/hooks follow the session root.** `settings.json` + hooks load from the session root (+ user-global `~/.claude` + managed). *Adding* a working directory or a plain shell `cd` does **not** re-key or reload them; the **`/cd` command** relocates the session to the new dir's project storage (v2.1.169+). So a plugin-local `.claude/settings.json`/hook is live only when the session is actually **rooted at that plugin**. (Historically, while everything ran rooted at DsonParser, plugin-local hooks were dormant and had to be wired user-global with an absolute path + a path-filter — see [[orientation-docs-restructuring]] — that workaround is only needed for seats you don't root at directly.)

**Memory architecture (as of 2026-07-12).** Auto-memory is keyed per **git repo / session root** (`~/.claude/projects/<cwd-key>/memory/`, the cwd path with non-alphanumerics → `-`). The four seats are separate repos, so each gets its **own store** when rooted there. Previously, always-rooting-at-DsonParser piled every seat's memories into the one `E--Work-Code-DsonTest2` store; on **2026-07-12 the plugin memories were migrated out** to per-repo stores (DsonToUnreal → `…-DsonHost-Plugins-DsonToUnreal/memory`; DsonArtisan → `…-DsonArtisanHost-Plugins-DsonArtisan/memory`), leaving the DsonParser store holding only parser + cross-cutting memories. (The two `project_*`/`reference_*` orphan stores from old Rider sessions were retired the same day; durable bits salvaged to `memory-maintenance/`.)

**Layered model going forward:** universal disciplines + identity → user-global `~/.claude/CLAUDE.md` (loaded in *every* session, all repos, independent of the memory store); each repo's durable rules → its own `CLAUDE.md`; per-repo `memory/` holds only that seat's evolving, incident-based facts. The `MEMORY.md` index has a hard **25 KB / 200-line** load cap (not configurable) — keep it lean; the split fixed a cap overflow. Any cross-seat memory predating the split still warrants seat-filtering. Prune/migration working docs live in `E:\Work\Code\DsonTest2\memory-maintenance\` (gitignored). See [[dsonartisan-upstream-opaque]], [[upstream-status-stale-confirm-with-user]], [[dsontounreal-seat-scope-boundary]].
