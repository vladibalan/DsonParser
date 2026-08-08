# Memory-autocommit hook (local convenience; NOT a verbatim framework core - see
# Docs/MemoryConventions.md "Autocommit"). Stop-event hook: when the agent stops, it commits ONLY
# the in-repo auto-memory store (.claude/memory/) so notes are never lost. It NEVER touches any
# other working-tree or staged change - the add and the commit are both limited to the store's
# pathspec, so unrelated work in the tree is left exactly as it was.
# Authored to the framework's documented Stop-hook behavior (the master ships an equivalent
# hooks/memory-autocommit.ps1; recorded as a local landing in Docs/FrameworkSlots.md).
# Targets Windows PowerShell 5.1 (no pwsh dependency). ASCII only. Fail-open: always exits 0 so it
# can never block the session from stopping.
$ErrorActionPreference = 'SilentlyContinue'

# repo root = two levels up from <repo>/.claude/hooks/<this script> (same derivation the guards use)
$repo  = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$store = '.claude/memory'   # repo-relative POSIX pathspec - the ONLY thing this hook ever commits

try {
    Set-Location -LiteralPath $repo -ErrorAction Stop

    # only act inside a real work tree
    $inside = & git rev-parse --is-inside-work-tree 2>$null
    if ($LASTEXITCODE -ne 0 -or "$inside".Trim() -ne 'true') { exit 0 }
    if (-not (Test-Path -LiteralPath (Join-Path $repo $store))) { exit 0 }

    # anything to commit under the store? (untracked, modified, and deleted all surface here)
    $changed = & git status --porcelain -- $store 2>$null
    if ([string]::IsNullOrWhiteSpace([string]$changed)) { exit 0 }

    # stage and commit ONLY the store pathspec. '-- $store' on the commit isolates it from any
    # other staged or unstaged change elsewhere in the tree.
    & git add -A -- $store 2>$null | Out-Null
    & git commit -m 'chore(memory): autocommit auto-memory store' -- $store 2>$null | Out-Null
} catch { }

exit 0
