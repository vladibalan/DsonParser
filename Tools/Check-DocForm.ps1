# Repo-wide doc-form census (verbatim framework core; text layer) - the backstop for writes that
# bypass the agent's Edit/Write hook (bulk scripts, human editors). Reads the SAME
# Tools/DocForm.config.json the doc-guard hook reads, so the two enforcers cannot drift; this script
# ships byte-identical to every consumer (framework D4a).
#   FAIL (exit 1)  - any line over the hard cap: agent Read truncates there, the overflow is
#                    INVISIBLE to an LLM (no stated-reason escape).
#   WARN (exit 0)  - any doc over the universal line/KB ceiling (sealed volumes exempt); hot-path
#                    budget breaches; lines over the signal cap; any non-ASCII char.
#   note           - bare-LF line endings on disk, one aggregated line (informational; git normalizes on add).
# -Strict promotes warns to failures (publish-gate mode). PS 5.1, ASCII, read-only.
# -Root / -ConfigPath override discovery (default: the repo containing this Tools/ dir + its config).
param(
    [switch]$Strict,
    [string]$Root,
    [string]$ConfigPath
)
$ErrorActionPreference = 'Stop'

if (-not $ConfigPath) { $ConfigPath = Join-Path $PSScriptRoot 'DocForm.config.json' }
if (-not $Root) { $Root = Split-Path $PSScriptRoot -Parent }
$cfg = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
$SealedDirs = @($cfg.sealedDirs)
$LineSignal = [int]$cfg.lineSignal
$LineHard   = [int]$cfg.lineHard
$MaxLines   = [int]$cfg.maxLines
$MaxKB      = [double]$cfg.maxKB
# budget keys are repo-relative POSIX paths; @()-wrap so -contains works for a single-entry table
$budgetNames = @(); if ($cfg.budgets) { $budgetNames = @($cfg.budgets.PSObject.Properties.Name) }

$utf8 = New-Object System.Text.UTF8Encoding($false)
$files = Get-ChildItem -LiteralPath $Root -Recurse -Filter *.md -File |
         Where-Object { $_.FullName -notmatch '\\(\.git|\.claude|\.codex|\.handoff)\\' }

$fails = 0; $warns = 0; $checked = 0; $lfDocs = 0; $lfLines = 0
foreach ($f in $files) {
    $checked++
    $rel = $f.FullName.Substring($Root.Length + 1)
    $relPosix = $rel -replace '\\', '/'
    $kb = [math]::Round($f.Length / 1KB, 1)
    $parent = Split-Path (Split-Path $f.FullName -Parent) -Leaf
    $isSealed = ($relPosix -match '^Docs/[^/]+/') -and ($SealedDirs -contains $parent)

    $text = [System.IO.File]::ReadAllText($f.FullName, $utf8)
    $bareLf = ([regex]::Matches($text, "(?<!`r)`n")).Count
    $nonAscii = ([regex]::Matches($text, '[^\x00-\x7F]')).Count
    $lineCount = 0; $maxLen = 0; $over400 = 0; $over2000 = 0
    foreach ($line in ($text -split "`r?`n")) {
        $lineCount++
        if ($line.Length -gt $maxLen) { $maxLen = $line.Length }
        if ($line.Length -gt $LineSignal) { $over400++ }
        if ($line.Length -gt $LineHard) { $over2000++ }
    }
    if ($text.EndsWith("`n")) { $lineCount-- }   # newline-terminated file: drop the phantom split element

    if ($over2000 -gt 0) {
        $fails++
        "FAIL  $relPosix : $over2000 line(s) over $LineHard chars (max $maxLen) - INVISIBLE past agent Read truncation. Wrap now."
    }
    $budget = $null
    if ($budgetNames -contains $relPosix) { $budget = $cfg.budgets.$relPosix }
    if ((-not $isSealed) -and (($lineCount -gt $MaxLines) -or ($kb -gt $MaxKB))) {
        $warns++
        "warn  $relPosix : $lineCount ln / $kb KB over the universal <=$MaxLines-line / <=$MaxKB KB ceiling (binds every doc) - relocate, topic-split, or rotate to a sealed volume."
    }
    if ($budget) {
        if (($lineCount -gt [int]$budget.lines) -or ($kb -gt [double]$budget.kb)) {
            $warns++
            "warn  $relPosix : $lineCount ln / $kb KB over hot-path budget $([int]$budget.lines) ln / $([double]$budget.kb) KB - relocate or split."
        }
    }
    if ($over400 -gt 0 -and $over2000 -eq 0 -and (-not $isSealed)) {
        $warns++
        "warn  $relPosix : $over400 line(s) over $LineSignal chars (max $maxLen) - hard-wrap prose at ~120 chars."
    }
    if ($nonAscii -gt 0) {
        $warns++
        "warn  $relPosix : $nonAscii non-ASCII char(s) - ASCII-only rule; cp1252 mojibake hazard."
    }
    if ($bareLf -gt 0) { $lfDocs++; $lfLines += $bareLf }
}

if ($lfDocs -gt 0) {
    "note  bare-LF line endings on disk in $lfDocs doc(s), $lfLines line(s) total (benign; git normalizes)."
}

''
"checked $checked docs: $fails hard-cap failure(s), $warns warning(s)."
if ($fails -gt 0) { 'RESULT: FAIL (hard cap).'; exit 1 }
if ($Strict -and $warns -gt 0) { 'RESULT: FAIL (-Strict: warnings gate).'; exit 1 }
'RESULT: PASS.'
exit 0
