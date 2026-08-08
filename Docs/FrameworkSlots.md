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
| `{{CPP_STANDARD}}` | `14`, pinned `/std:c++14` via root `Directory.Build.props` (CPP-1; was v143 default) |
| `{{SOLUTION}}` / `{{CONFIG}}` / `{{PLATFORM}}` | `DsonTest2` / `Release` / `x64` |
| `{{VENDORED_DIR}}` | `DsonParser/include/rapidjson` |
| include-guard `owningHeaders` | `{}` (plain C++); the hook itself is OFF (checkpoint 2a) |
| native-binding module | LIVE - `Docs/NativeBinding.md` (CPP-3/4) |
| versioning module | LIVE - `Docs/Versioning.md` + `Tools/ReleaseTag.config.json` |

## Local deviations from the stock MATERIALIZE

Governed by COD-102 (`Docs/Rulebook.md`): conformance is the default; every entry below is a
deliberate, argued departure from the stock procedure - the concrete constraint and its trade-off -
never a least-effort default.

Note (2026-08-08): the closed-catalog conformance model these deviations answer to is under owner reframe
toward consumer-selective interaction, so COD-102 is currently paused; status + why in `Docs/Roadmap.md`
and `Docs/DecisionLog.md`.

- cpp `Patterns.md` core NOT vendored: filename collision with the text-layer core (both target
  `Docs/Patterns.md`). Deferred to the next sync; fit gap carried to the master. Its conventions
  (orientation headers, vendoring fence, `VENDORED.txt` seal) are applied in-tree regardless.
- The repo's previous `Check-ReleaseTag.ps1` had an `-Audit` mode (a full sweep: every released
  CHANGELOG heading against its tag); the config-driven core (`Tools/Check-ReleaseTag.ps1`) lacks it
  and gates only the current release point (version source, top CHANGELOG heading, and git tag all
  agree). Trade-off: the interim loss is that historical heading/tag drift is no longer swept
  wholesale; each new release is still gated at its own release point by the COD-11 close-gate, with
  Director tagging discipline the backstop, so the residual risk is bounded to pre-existing releases.
  Accepted; fit gap carried to the master (restore the sweep upstream).
- `Tools/IncludeGuard.config.json` lands with `repoDirMarker` deliberately blank = include-guard
  disabled (checkpoint 2a); the hook script is not vendored into `.claude/hooks/`.
- memory-autocommit hook wired (2026-08-08): the Claude auto-memory store was relocated in-repo to
  `.claude/memory/` via the `autoMemoryDirectory` binding in `.claude/settings.local.json` - a
  per-seat local binding whose value is an absolute machine path. The Stop-hook
  `.claude/hooks/memory-autocommit.ps1` (authored to the master's spec; a local convenience, not a
  verbatim core) commits only the store's pathspec on session end.
- Test-runner cores (`RunTests.ps1`, `TestConfig.local.example.json`) not vendored: the testing
  module is dormant (JIT - stand up with the first suite).
- `Capabilities.md` materialized 2026-08-08: the capability catalog lives in `Docs/Capabilities.md`
  (promoted from the retired `DsonParser_Roadmap.md`); `dson-parsing-overview.md` stays the
  parse-behavior map. `Intent`/`Principles` landed 2026-08-08, owner-approved.
- No SLOTS.md / MATERIALIZE.md copies kept - this doc is the adaptation record.
- All other landings per the stock procedure, at canonical `Docs/` / `Tools/` casing (the previous
  lowercase `docs/` / `tools/` dirs were renamed in the migration).
