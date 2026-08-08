# Doc-economy guard (verbatim framework core; text layer). Reads ALL project-specific state from
# <repo-root>/Tools/DocForm.config.json, so this script ships BYTE-IDENTICAL to every consumer - a
# true verbatim core (framework D4a). The census Tools/Check-DocForm.ps1 reads the SAME config, so
# the two enforcers cannot drift.
# Fires on Edit/Write/MultiEdit to any repo .md and self-filters out the rest.
#   PreToolUse  -> injects the tiering + form reminder.
#   PostToolUse -> warns on budget/form breaches; BLOCKS a save that crosses the hard line cap
#                  (agent Read truncates there - the overflow is invisible).
# Targets Windows PowerShell 5.1 (no pwsh dependency). ASCII only. Reads payload from stdin.
$ErrorActionPreference = 'Stop'

# --- config: the only per-project state (this script itself stays byte-identical) ----------------
$configPath = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'Tools/DocForm.config.json'
if (-not (Test-Path -LiteralPath $configPath)) { exit 0 }   # fail-open: the census is the backstop
try { $cfg = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json } catch { exit 0 }
$PathMarker = [string]$cfg.pathMarker
if ([string]::IsNullOrWhiteSpace($PathMarker)) { exit 0 }
$SealedDirs = @($cfg.sealedDirs)
$LineSignal = [int]$cfg.lineSignal
$LineHard   = [int]$cfg.lineHard
$MaxLines   = [int]$cfg.maxLines
$MaxKB      = [double]$cfg.maxKB
# budget keys are repo-relative POSIX paths; @()-wrap so -contains works for a single-entry table
$budgetNames = @(); if ($cfg.budgets) { $budgetNames = @($cfg.budgets.PSObject.Properties.Name) }

$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }
try { $payload = $raw | ConvertFrom-Json } catch { exit 0 }
$path = if ($payload.tool_input) { [string]$payload.tool_input.file_path } else { '' }
if ([string]::IsNullOrWhiteSpace($path)) { exit 0 }
$event = if ($payload.hook_event_name) { [string]$payload.hook_event_name } else { 'PreToolUse' }

$norm = $path -replace '\\', '/'
$markerRe = '/' + [regex]::Escape($PathMarker) + '/'
if ($norm -notmatch $markerRe) { exit 0 }
if ($norm -match '/\.claude/' -or $norm -match '/\.codex/' -or $norm -match '/\.handoff/') { exit 0 }
if ($norm -notmatch '\.md$') { exit 0 }

$rel = $norm -replace ('^.*' + $markerRe), ''   # repo-relative POSIX path (budget key)
$leaf = Split-Path $path -Leaf
$isSealed = $false
if ($norm -match '/Docs/([^/]+)/.+\.md$') { $isSealed = $SealedDirs -contains $Matches[1] }

$budget = $null
if ($budgetNames -contains $rel) { $budget = $cfg.budgets.$rel }

function Write-Context([string]$eventName, [string]$text) {
    $out = @{ hookSpecificOutput = @{ hookEventName = $eventName; additionalContext = $text } }
    $out | ConvertTo-Json -Depth 5 -Compress
}

if ($event -eq 'PostToolUse') {
    $warns = @()
    if ($isSealed) {
        $warns += "Sealed volume: $leaf is a rotated cold-doc volume - immutable once sealed. New entries belong in the root doc."
    }
    $bytes = 0; $lineCount = 0; $maxLen = 0; $over400 = 0; $over2000 = 0; $nonAscii = 0
    try {
        $bytes = (Get-Item -LiteralPath $path -ErrorAction Stop).Length
        foreach ($line in [System.IO.File]::ReadLines($path)) {
            $lineCount++
            $len = $line.Length
            if ($len -gt $maxLen) { $maxLen = $len }
            if ($len -gt $LineSignal) { $over400++ }
            if ($len -gt $LineHard) { $over2000++ }
            $nonAscii += ([regex]::Matches($line, '[^\x00-\x7F]')).Count
        }
    } catch { exit 0 }
    $kb = [math]::Round($bytes / 1KB, 1)

    if ((-not $isSealed) -and (($lineCount -gt $MaxLines) -or ($kb -gt $MaxKB))) {
        $warns += "Universal ceiling: $leaf is now $lineCount lines / $kb KB, over the <=$MaxLines-line AND <=$MaxKB KB ceiling that binds every doc (cold included). Relocate, topic-split, or rotate an append-only log's oldest entries to a sealed volume behind a root index."
    }
    if ($budget) {
        if (($lineCount -gt [int]$budget.lines) -or ($kb -gt [double]$budget.kb)) {
            $warns += "Hot-path budget: $leaf is now $lineCount lines / $kb KB against a soft budget of $([int]$budget.lines) lines / $([double]$budget.kb) KB. Re-bloat signal: relocate content to its tier or split - do not keep growing."
        }
    }
    if ($over400 -gt 0) {
        $warns += "Line length: $leaf has $over400 line(s) over $LineSignal chars (max $maxLen; $over2000 over the $LineHard hard cap). Hard-wrap prose at ~120 chars; paragraph-scale table cells become heading + wrapped prose."
    }
    if ($nonAscii -gt 0) {
        $warns += "ASCII: $leaf contains $nonAscii non-ASCII char(s) - cp1252 mojibake hazard. Replace with ASCII equivalents."
    }
    if ($over2000 -gt 0) {
        $msg = "HARD CAP: $leaf has $over2000 line(s) over $LineHard chars (max $maxLen) - content past $LineHard chars is INVISIBLE to agent Read. Wrap those lines NOW (~120-char prose) before continuing. " + ($warns -join ' | ')
        @{ decision = 'block'; reason = $msg } | ConvertTo-Json -Compress
        exit 0
    }
    if ($warns.Count -eq 0) { exit 0 }
    Write-Context 'PostToolUse' ($warns -join ' | ')
    exit 0
}

$reminder = @'
Doc economy (the doc-rules core) for this edit:
- One tier per doc - put content in its home: entry -> AGENTS.md (canonical) + CLAUDE.md (adapter); forward direction -> Intent; open status -> Roadmap; settled dated rationale -> DecisionLog; durable facts/lessons -> Reference; doc form + tiering -> the doc-rules doc; the role-free disciplines -> the disciplines doc.
- Point, don't duplicate: if another doc owns a fact, link it by name.
- Form: ASCII only (cp1252 mojibake hazard); hard-wrap prose at ~120 chars - the signal cap is a defect flag, the hard cap is invisible past agent Read truncation and hook-blocked; tables hold short enumerable facts only; log-shaped content under dated grep-able headings.
- Every doc stays within the universal ceiling (lines AND KB); hot docs carry tighter soft budgets (Tools/DocForm.config.json). Cold docs rotate or topic-split at the ceiling.
'@
if ($isSealed) {
    $reminder = "SEALED VOLUME: this file is a rotated cold-doc volume - immutable once sealed; new entries belong in the root doc.`n" + $reminder
}
Write-Context 'PreToolUse' $reminder
exit 0
