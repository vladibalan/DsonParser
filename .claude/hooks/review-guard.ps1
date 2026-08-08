# Code-review guard (verbatim framework core; code layer). Reads ALL project-specific state from
# <repo-root>/Tools/ReviewGuard.config.json, so this script ships BYTE-IDENTICAL to every consumer - a
# true verbatim core (framework D4a), the code-layer sibling of the text layer's doc-guard. The SAME
# file runs from .claude/hooks/ (Claude Code) and .codex/hooks/ (the Codex mirror); it computes the
# config path relative to itself, so both copies stay byte-identical (DecisionLog D17).
# Fires on Edit/Write/MultiEdit and self-filters to source files only:
#   PreToolUse -> injects the COD-1..9 code-review self-check as additionalContext, so the agent
#                 self-audits each diff. (No PostToolUse: the rulebook is a reminder, not a measure.)
# Targets Windows PowerShell 5.1 (no pwsh dependency). ASCII only. Reads payload from stdin.
$ErrorActionPreference = 'Stop'

# --- config: the only per-project state (this script itself stays byte-identical) ----------------
$configPath = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'Tools/ReviewGuard.config.json'
if (-not (Test-Path -LiteralPath $configPath)) { exit 0 }   # fail-open: review is a reminder, not a gate
try { $cfg = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json } catch { exit 0 }
$RepoDirMarker = [string]$cfg.repoDirMarker
if ([string]::IsNullOrWhiteSpace($RepoDirMarker)) { exit 0 }
$SourceMarkers = @($cfg.sourceMarkers)
$SourceExts    = @($cfg.sourceExtensions)
if ($SourceMarkers.Count -eq 0 -or $SourceExts.Count -eq 0) { exit 0 }
$ExtraRules = @(); if ($cfg.extraRules) { $ExtraRules = @($cfg.extraRules) }

$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }
try { $payload = $raw | ConvertFrom-Json } catch { exit 0 }
$path = if ($payload.tool_input) { [string]$payload.tool_input.file_path } else { '' }
if ([string]::IsNullOrWhiteSpace($path)) { exit 0 }

$norm = $path -replace '\\', '/'
if ($norm -notmatch ('/' + [regex]::Escape($RepoDirMarker) + '/')) { exit 0 }
if ($norm -match '/\.claude/' -or $norm -match '/\.codex/' -or $norm -match '/\.handoff/') { exit 0 }

# must sit under a configured source root ...
$inSource = $false
foreach ($m in $SourceMarkers) {
    $frag = ([string]$m -replace '\\', '/').TrimEnd('/')
    if ([string]::IsNullOrWhiteSpace($frag)) { continue }
    if ($norm -match [regex]::Escape($frag)) { $inSource = $true; break }
}
if (-not $inSource) { exit 0 }

# ... and carry a configured source extension
$extAlt = ($SourceExts | ForEach-Object { [regex]::Escape(([string]$_).TrimStart('.')) }) -join '|'
if ([string]::IsNullOrWhiteSpace($extAlt)) { exit 0 }
if ($norm -notmatch ('\.(' + $extAlt + ')$')) { exit 0 }

$checklist = @'
Code-review self-check (the code rulebook, COD-1..9) for this edit:
COD-1 No duplicated logic - reuse the shared helpers/seams; extend the nearest existing seam over a fresh design; never re-inline a correctness-critical transform; report any unavoidable copy in the feedback, never silently.
COD-2 Compact without losing behavior - no dead params, no consumer-less generated/annotation declarations, no debug scaffolding in the hot path; confirm nothing load-bearing before deleting; a retained quirk carries a one-line WHY.
COD-3 Match the file's idiom - mirror the surrounding patterns; unique grep-able names; file named for its primary unit; build stays warning-clean.
COD-4 Contracts - builders/entry points return a failure sentinel with enough context to reproduce; public interfaces state responsibility/lifetime/call-order/failure; a public-signature (breaking) change is called out explicitly, never slipped into a minor edit; permissive ingest where external data is read.
COD-5 Size - a function past ~100 lines or a file past ~1,000 splits at a named seam unless a stated reason says it is genuinely linear; never delete a comment or diagnostic to fit a budget.
COD-6 Nesting - control-flow depth stays <= 4 (body 0, first control block 1); depth >= 5 is refactored with a helper or guard clause. Hard cap, no stated-reason escape.
COD-7 Logic lives in text - no logic in shipped carriers; no string-based code dispatch; no new hand-rolled singletons or mutable statics.
COD-8 Tests are source - obey every rule here; each assertion compares a task-file spec literal (never a code-under-test value); non-vacuity asserted; an absent-fixture test skips loudly by name; never hand-edit an expectation to force green.
COD-9 If reviewing - lead with findings by severity + file:line; enumerate (grep the family), do not gesture; separate must-fix from dead-but-harmless; call out silent behavior shifts.
After editing, state which of COD-1..9 you checked - do not just say "looks fine".
'@
if ($ExtraRules.Count -gt 0) { $checklist = $checklist + "`n" + (($ExtraRules | ForEach-Object { [string]$_ }) -join "`n") }

$out = @{ hookSpecificOutput = @{ hookEventName = 'PreToolUse'; additionalContext = $checklist } }
$out | ConvertTo-Json -Depth 5 -Compress
exit 0
