<#
Check-ReleaseTag.ps1 -- DsonParser release-tag close-gate.

Verifies that the library's current version has its matching lightweight git tag
(vX.Y.Z). The tag is the one release carrier NOT in the working tree, so it is
invisible to git diff / the build / the review and is the easiest release step to
miss. The Director runs this before reporting any surface-touching task done.

  Check-ReleaseTag.ps1          Check the current DSONPARSER_VERSION_STRING has
                                its vX.Y.Z tag. Exit 0 if tagged, 1 if missing
                                (prints the exact git tag command), 2 on error.
  Check-ReleaseTag.ps1 -Audit   Cross-check every released CHANGELOG.md heading
                                (## X.Y.Z ...) has a tag; list any that do not.

Windows PowerShell 5.1 compatible; ASCII only. Git must be on PATH. Paths and git
calls resolve against the repo root (this script's parent of Tools/), so it works
from any working directory. See docs/agent-workflow.md "Git & release tagging" and
docs/versioning.md.
#>

[CmdletBinding()]
param(
    [switch]$Audit
)

$ErrorActionPreference = 'Stop'

# Resolve repo paths relative to this script (Tools/ sits directly under the root).
$root       = Split-Path -Parent $PSScriptRoot
$headerPath = Join-Path $root 'DsonParser\DsonParserVersion.h'
$changePath = Join-Path $root 'CHANGELOG.md'

function Test-TagExists([string]$repo, [string]$tag) {
    $found = git -C $repo tag --list $tag
    return -not [string]::IsNullOrWhiteSpace($found)
}

if (-not (Test-Path -LiteralPath $headerPath)) {
    Write-Host "ERROR: version header not found: $headerPath"
    exit 2
}

$headerText = Get-Content -Raw -LiteralPath $headerPath
if ($headerText -notmatch 'DSONPARSER_VERSION_STRING\s+"([0-9]+\.[0-9]+\.[0-9]+)"') {
    Write-Host "ERROR: DSONPARSER_VERSION_STRING not found in $headerPath"
    exit 2
}
$version = $Matches[1]

if ($Audit) {
    if (-not (Test-Path -LiteralPath $changePath)) {
        Write-Host "ERROR: CHANGELOG not found: $changePath"
        exit 2
    }
    $headings = @(
        Select-String -LiteralPath $changePath -Pattern '^##\s+([0-9]+\.[0-9]+\.[0-9]+)' |
            ForEach-Object { $_.Matches[0].Groups[1].Value } |
            Select-Object -Unique
    )
    $missing = @()
    foreach ($v in $headings) {
        if (-not (Test-TagExists $root "v$v")) { $missing += $v }
    }
    Write-Host ("Audit: {0} released CHANGELOG version(s), {1} untagged." -f $headings.Count, $missing.Count)
    if ($missing.Count -gt 0) {
        foreach ($v in $missing) { Write-Host "  MISSING tag: v$v" }
        Write-Host ("Tag each on its release commit, e.g.: git tag v{0} <release-commit>" -f $missing[0])
        exit 1
    }
    Write-Host "OK: every released CHANGELOG version has a tag."
    exit 0
}

# Default mode: current version only.
$tag = "v$version"
if (Test-TagExists $root $tag) {
    Write-Host "OK: current version $version is tagged ($tag)."
    exit 0
}

$commit = git -C $root log -1 --format=%H -- 'DsonParser/DsonParserVersion.h'
Write-Host "MISSING release tag for current version $version."
if ([string]::IsNullOrWhiteSpace($commit)) {
    Write-Host "  git tag $tag <release-commit>"
} else {
    $short = $commit.Substring(0, 9)
    Write-Host "  Release commit (last touched the version macro): $short"
    Write-Host "  git tag $tag $commit"
}
Write-Host "  git tag --list $tag        # confirm"
exit 1
