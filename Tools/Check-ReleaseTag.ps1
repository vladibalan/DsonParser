# Release-carrier close-gate check (the versioning module close-gate, COD-11). Verbatim framework
# core; code layer, config-externalized (D4a). Cross-checks the three release carriers agree at the
# current release point:
#   1. the version literal in the configured version source (the single source of truth)
#   2. the top release heading in the CHANGELOG (## X.Y.Z ...)
#   3. the git tag vX.Y.Z (exists; notes whether it is on HEAD)
# Reads the per-project version-source descriptor from Tools/ReleaseTag.config.json (beside this
# script), so the script ships BYTE-IDENTICAL to every consumer. Part of the dormant/JIT versioning
# set - present only once the project stands up release carriers. Run from anywhere; paths resolve
# relative to this script. ASCII only; Windows PowerShell 5.1. Exit 0 = consistent; 1 = mismatch/missing.
$ErrorActionPreference = 'Stop'

$RepoRoot   = Split-Path -Parent $PSScriptRoot
$configPath = Join-Path $PSScriptRoot 'ReleaseTag.config.json'
if (-not (Test-Path -LiteralPath $configPath)) {
    Write-Host 'FAIL: Tools/ReleaseTag.config.json not found - fill the version-source descriptor when standing up the versioning carriers.'
    exit 1
}
try { $cfg = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json } catch {
    Write-Host 'FAIL: ReleaseTag.config.json is not valid JSON.'; exit 1
}
$verRel = [string]$cfg.versionSourceFile
$verPat = [string]$cfg.versionSourcePattern
if ([string]::IsNullOrWhiteSpace($verRel) -or [string]::IsNullOrWhiteSpace($verPat) -or $verRel -like '*{{*') {
    Write-Host 'FAIL: ReleaseTag.config.json version-source descriptor is unfilled ({{...}} placeholder).'; exit 1
}
$verFile = Join-Path $RepoRoot $verRel
$clRel   = if ($cfg.changelogPath) { [string]$cfg.changelogPath } else { 'CHANGELOG.md' }
$clPath  = Join-Path $RepoRoot $clRel

$fail = @()

# 1. version source of truth (the configured file + capture-group pattern)
$vn = $null
if (Test-Path -LiteralPath $verFile) {
    $m = Select-String -LiteralPath $verFile -Pattern $verPat | Select-Object -First 1
    if ($m -and $m.Matches[0].Groups.Count -ge 2) { $vn = $m.Matches[0].Groups[1].Value }
}
if (-not $vn) { Write-Host ('FAIL: no version match for the configured pattern in ' + $verRel); exit 1 }
Write-Host ('version source : ' + $vn + '  (' + $verRel + ')')

# 2. CHANGELOG top release heading
if (-not (Test-Path -LiteralPath $clPath)) { $fail += ($clRel + ' not found') }
else {
    $cl = $null
    $m = Select-String -LiteralPath $clPath -Pattern '^##\s+(\d+\.\d+\.\d+)' | Select-Object -First 1
    if ($m) { $cl = $m.Matches[0].Groups[1].Value }
    if (-not $cl) { $fail += ($clRel + ' has no release heading (## X.Y.Z ...)') }
    elseif ($cl -ne $vn) { $fail += ('CHANGELOG top heading ' + $cl + ' != version source ' + $vn) }
    else { Write-Host ('CHANGELOG top  : ' + $cl) }
}

# 3. git tag vX.Y.Z (the one carrier invisible to git diff)
$tag = 'v' + $vn
Push-Location $RepoRoot
try {
    $found = git tag --list $tag 2>$null
    if (-not $found) {
        $fail += ('git tag ' + $tag + ' does not exist - create it on the release commit: git tag -a ' + $tag + ' -m <summary>')
    } else {
        $tagCommit  = git rev-list -n 1 $tag 2>$null
        $headCommit = git rev-parse HEAD 2>$null
        if ($tagCommit -eq $headCommit) { Write-Host ('git tag        : ' + $tag + ' (on HEAD)') }
        else { Write-Host ('git tag        : ' + $tag + ' (not on HEAD - fine if HEAD moved past the release commit)') }
    }
} finally { Pop-Location }

if ($fail.Count -gt 0) {
    $fail | ForEach-Object { Write-Host ('FAIL: ' + $_) }
    exit 1
}
Write-Host 'PASS: release carriers consistent.'
exit 0
