<#
gen-compile-commands.ps1

Generates a clangd-compatible compile_commands.json at the repo root for the
DsonParser (DLL) and DsonTest2 (console) targets.

The solution is MSBuild-only, and MSVC's cl.exe does not emit a compilation
database. Rather than depend on a build run (builds are user-run here), this
script reconstructs the per-target compile flags that the .vcxproj files pin
for the Release|x64 configuration -- the documented "typical build":

    msbuild DsonTest2.sln /p:Configuration=Release /p:Platform=x64

It then enumerates each project's .cpp translation units and writes one entry
per TU. Entries use the `clang-cl` driver so clangd parses the MSVC-style `/`
flags (defines/-D, includes/-I, /std:c++14) correctly.

Regenerate after adding/removing .cpp files or changing include dirs/defines:

    pwsh -File tools/gen-compile-commands.ps1
    # or, on Windows PowerShell:
    powershell -ExecutionPolicy Bypass -File tools/gen-compile-commands.ps1

Output: compile_commands.json (repo root). Do not hand-edit it.
#>

$ErrorActionPreference = 'Stop'

# Repo root = parent of this script's directory (tools/..).
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRootFwd = $repoRoot -replace '\\', '/'

# Shared flags for both targets. C++14 is deliberate (MSVC v143 default); see
# docs/code-review-rules.md R4.1. /EHsc matches the implicit MSVC C++ default.
$commonFlags = @('/std:c++14', '/EHsc')

# Per-target configuration mirrors the Release|x64 ItemDefinitionGroup in each
# .vcxproj. `dir` is the project directory; `cpp` TUs are discovered by glob so
# the database stays correct as files are added or removed.
$targets = @(
    @{
        Name     = 'DsonParser'
        Dir      = Join-Path $repoRoot 'DsonParser'
        Defines  = @('WIN32', 'NDEBUG', 'DSONPARSER_EXPORTS', '_WINDOWS', '_USRDLL')
        # Project dir (for "DsonTypes.h" etc.) + vendored rapidjson include root.
        Includes = @('DsonParser', 'DsonParser/include')
    },
    @{
        Name     = 'DsonTest2'
        Dir      = Join-Path $repoRoot 'DsonTest2'
        Defines  = @('WIN32', 'NDEBUG', '_CONSOLE')
        # Solution dir's DsonParser folder, for the public C ABI header.
        Includes = @('DsonParser')
    }
)

$entries = New-Object System.Collections.Generic.List[object]

foreach ($t in $targets) {
    $defineFlags  = $t.Defines  | ForEach-Object { "/D$_" }
    $includeFlags = $t.Includes | ForEach-Object { "/I$repoRootFwd/$_" }

    # Sorted for stable, reviewable diffs across regenerations.
    $cppFiles = Get-ChildItem -Path $t.Dir -Filter '*.cpp' -File | Sort-Object Name
    foreach ($cpp in $cppFiles) {
        $fileFwd = $cpp.FullName -replace '\\', '/'
        $args = @('clang-cl') + $commonFlags + $defineFlags + $includeFlags + @('/c', $fileFwd)
        $entries.Add([ordered]@{
            directory = $repoRootFwd
            file      = $fileFwd
            arguments = $args
        })
    }
}

$json = ConvertTo-Json -InputObject $entries -Depth 5
$outPath = Join-Path $repoRoot 'compile_commands.json'
# UTF-8 without BOM; trailing newline.
[System.IO.File]::WriteAllText($outPath, $json + "`n", (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Wrote $($entries.Count) entries to $outPath"
