# DsonParser - Framework Adaptation Record (LLMfrmw SLOTS)

The filled adaptation slots for the three materialized layers - the SLOTS record the migration
keeps per the master's `layers/MigrationGuide.md` - plus every local deviation from the stock
procedure. Pin record: `Docs/Reference.md` "Framework pin record". Cold/on-demand.

## Filled slots (text + code + cpp)

| Slot | Value |
| --- | --- |
| `{{PROJECT_NAME}}` | DsonParser (DsonTest2 = harness + solution name) |
| `{{REPO_DIR_MARKER}}` | `DsonTest2` |
| `{{BASE_BRANCH}}` | `main` |
| `{{AGENT_GUIDE}}` | `AGENTS.md` |
| `{{SOURCE_ROOT}}` | `DsonParser/`, `DsonTest2/`, `DsonLoadTest/` |
| `{{PROJECT_DIR}}` | n/a - no `.codex/` mirror (checkpoint 2e) |
| `{{BUILD_VERIFY_CMD}}` | msbuild-via-vswhere, `DsonTest2.sln` Release x64 (`Docs/Tooling.md`) |
| `{{UPSTREAM_REPO}}` | `Tencent/rapidjson` (canonical pin: `Docs/Reference.md`) |
| `{{VERSION_SOURCE_FILE}}` + pattern | `DsonParser/DsonParserVersion.h`, `DSONPARSER_VERSION_STRING\s+"([^"]+)"` |
| `{{TEST_COMMAND}}` | dormant - no automated suite (COD-8 stays JIT) |
| `{{CPP_STANDARD}}` | `14` (effective via the v143 default; explicit pin = Roadmap backlog) |
| `{{SOLUTION}}` / `{{CONFIG}}` / `{{PLATFORM}}` | `DsonTest2` / `Release` / `x64` |
| `{{VENDORED_DIR}}` | `DsonParser/include/rapidjson` |
| include-guard `owningHeaders` | `{}` (plain C++); the hook itself is OFF (checkpoint 2a) |
| native-binding module | LIVE - `Docs/NativeBinding.md` (CPP-3/4) |
| versioning module | LIVE - `Docs/Versioning.md` + `Tools/ReleaseTag.config.json` |

## Local deviations from the stock MATERIALIZE

- cpp `Patterns.md` core NOT vendored: filename collision with the text-layer core (both target
  `Docs/Patterns.md`). Deferred to the next sync; fit gap carried to the master. Its conventions
  (orientation headers, vendoring fence, `VENDORED.txt` seal) are applied in-tree regardless.
- The repo's previous `Check-ReleaseTag.ps1` had an `-Audit` mode (all released headings vs tags);
  the config-driven core lacks it. Accepted; fit gap carried to the master.
- `Tools/IncludeGuard.config.json` lands with `repoDirMarker` deliberately blank = include-guard
  disabled (checkpoint 2a); the hook script is not vendored into `.claude/hooks/`.
- memory-autocommit hook not wired: the Claude memory store lives outside the repo (bringing it
  in-repo is a `Docs/Roadmap.md` backlog item).
- Test-runner cores (`RunTests.ps1`, `TestConfig.local.example.json`) not vendored: the testing
  module is dormant (JIT - stand up with the first suite).
- `Capabilities.md` skeleton skipped: `DsonParser_Roadmap.md` owns capability detail (restructure
  deferred, `Docs/Roadmap.md`). `Intent`/`Principles` landed 2026-08-08, owner-approved.
- No SLOTS.md / MATERIALIZE.md copies kept - this doc is the adaptation record.
- All other landings per the stock procedure, at canonical `Docs/` / `Tools/` casing (the previous
  lowercase `docs/` / `tools/` dirs were renamed in the migration).
