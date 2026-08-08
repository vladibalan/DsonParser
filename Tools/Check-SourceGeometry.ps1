# Source-geometry census (verbatim framework core; cpp layer). The close-gate backstop for source
# geometry: COD-5 size ceilings (soft -> warn) and COD-6 control-flow nesting (a hard cap -> fail) - the
# source-side sibling of the text layer's Check-DocForm. Reads the SAME Tools/SourceGeometry.config.json
# a project fills, so this script ships BYTE-IDENTICAL to every consumer (framework D4a). Run at the
# close-gate (like Check-DocForm / Check-ReleaseTag), NOT as a per-edit hook.
#   FAIL (exit 1)  - a unit nests control flow past the cap (COD-6 hard cap, no stated-reason escape).
#   warn (exit 0)  - a file over its size ceiling, or a function over its length ceiling (COD-5 soft: a
#                    genuinely linear body may stay whole with a stated reason - review confirms).
# -Strict promotes warns to failures (publish/close-gate mode). PS 5.1, ASCII, read-only.
# -Root / -ConfigPath override discovery (default: the repo containing this Tools/ dir + its config).
#
# Measurement is a control-flow-aware HEURISTIC, not a compiler-grade parse: comments and string/char
# literals are stripped, then each `{` is classified by its statement head - a control keyword
# (if/for/while/switch/catch/else/do/try) opens a nesting level; a namespace/class/init-list/function
# body is structural. Lambdas and macro-embedded braces are approximated and nesting errs toward
# flagging; a flagged unit is a SIGNAL re-checked in review.
param(
    [switch]$Strict,
    [string]$Root,
    [string]$ConfigPath
)
$ErrorActionPreference = 'Stop'

if (-not $ConfigPath) { $ConfigPath = Join-Path $PSScriptRoot 'SourceGeometry.config.json' }
if (-not $Root) { $Root = Split-Path $PSScriptRoot -Parent }
$cfg = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
$SourceDirs = @($cfg.sourceDirs) | Where-Object { $_ }
$Exts = @($cfg.sourceExtensions) | Where-Object { $_ } | ForEach-Object { ([string]$_).TrimStart('.').ToLowerInvariant() }
if ($Exts.Count -eq 0) { $Exts = @('h', 'hpp', 'cpp', 'cc', 'cxx', 'c', 'inl') }
$ExcludeDirs = @()
if ($cfg.excludeDirs) { $ExcludeDirs = @($cfg.excludeDirs | Where-Object { $_ -and ([string]$_ -notmatch '{{') }) }
$FuncSignal = [int]$cfg.funcSignal; if ($FuncSignal -le 0) { $FuncSignal = 60 }
$FuncSplit  = [int]$cfg.funcSplit;  if ($FuncSplit  -le 0) { $FuncSplit  = 100 }
$FileSignal = [int]$cfg.fileSignal; if ($FileSignal -le 0) { $FileSignal = 800 }
$FileSplit  = [int]$cfg.fileSplit;  if ($FileSplit  -le 0) { $FileSplit  = 1000 }
$NestingCap = [int]$cfg.nestingCap; if ($NestingCap -le 0) { $NestingCap = 4 }

$utf8 = New-Object System.Text.UTF8Encoding($false)

$files = @()
foreach ($d in $SourceDirs) {
    $abs = Join-Path $Root ([string]$d)
    if (Test-Path -LiteralPath $abs) {
        $files += Get-ChildItem -LiteralPath $abs -Recurse -File |
            Where-Object { $Exts -contains $_.Extension.TrimStart('.').ToLowerInvariant() }
    }
}
if ($ExcludeDirs.Count -gt 0) {
    $files = $files | Where-Object {
        $p = $_.FullName -replace '\\', '/'
        $keep = $true
        foreach ($x in $ExcludeDirs) {
            $frag = ([string]$x -replace '\\', '/').Trim('/')
            if ($frag -and ($p -match ('/' + [regex]::Escape($frag) + '/'))) { $keep = $false; break }
        }
        $keep
    }
}
$files = @($files | Sort-Object FullName)

function Get-CleanText([string]$text) {
    # Blank out // line comments, /* */ block comments, and "..."/'...' literals; preserve newlines so
    # line numbers stay aligned with the source.
    $sb = New-Object System.Text.StringBuilder
    $n = $text.Length; $i = 0; $state = 'code'
    while ($i -lt $n) {
        $c = $text[$i]
        $c2 = if ($i + 1 -lt $n) { $text[$i + 1] } else { [char]0 }
        if ($state -eq 'code') {
            if ($c -eq '/' -and $c2 -eq '/') { $state = 'line'; [void]$sb.Append('  '); $i += 2 }
            elseif ($c -eq '/' -and $c2 -eq '*') { $state = 'block'; [void]$sb.Append('  '); $i += 2 }
            elseif ($c -eq '"') { $state = 'dq'; [void]$sb.Append(' '); $i++ }
            elseif ($c -eq "'") { $state = 'sq'; [void]$sb.Append(' '); $i++ }
            else { [void]$sb.Append($c); $i++ }
        }
        elseif ($state -eq 'line') {
            if ($c -eq "`n") { $state = 'code'; [void]$sb.Append("`n") } else { [void]$sb.Append(' ') }
            $i++
        }
        elseif ($state -eq 'block') {
            if ($c -eq '*' -and $c2 -eq '/') { $state = 'code'; [void]$sb.Append('  '); $i += 2 }
            else { if ($c -eq "`n") { [void]$sb.Append("`n") } else { [void]$sb.Append(' ') }; $i++ }
        }
        elseif ($state -eq 'dq') {
            if ($c -eq '\') { [void]$sb.Append('  '); $i += 2 }
            elseif ($c -eq '"') { $state = 'code'; [void]$sb.Append(' '); $i++ }
            else { if ($c -eq "`n") { [void]$sb.Append("`n") } else { [void]$sb.Append(' ') }; $i++ }
        }
        elseif ($state -eq 'sq') {
            if ($c -eq '\') { [void]$sb.Append('  '); $i += 2 }
            elseif ($c -eq "'") { $state = 'code'; [void]$sb.Append(' '); $i++ }
            else { if ($c -eq "`n") { [void]$sb.Append("`n") } else { [void]$sb.Append(' ') }; $i++ }
        }
    }
    $sb.ToString()
}

function Measure-Geometry([string]$clean) {
    $stack = New-Object System.Collections.Generic.Stack[object]
    $maxCtrl = 0; $maxCtrlLine = 0
    $funcs = @()
    $head = New-Object System.Text.StringBuilder
    $line = 1
    $paren = 0   # a for-loop head carries `;` inside parens; only a paren-depth-0 `;` ends a statement
    $ctrlKw = '\b(if|for|while|switch|catch)\b'
    $typeKw = '\b(namespace|class|struct|union|enum)\b'
    foreach ($ch in $clean.ToCharArray()) {
        if ($ch -eq "`n") { $line++; [void]$head.Append(' ') }
        elseif ($ch -eq '(') { $paren++; [void]$head.Append($ch) }
        elseif ($ch -eq ')') { if ($paren -gt 0) { $paren-- }; [void]$head.Append($ch) }
        elseif ($ch -eq '{') {
            $h = $head.ToString().Trim()
            $isControl = ($h -match $ctrlKw) -or ($h -match '(^|\W)else\s*$') -or ($h -match '(^|\W)do\s*$') -or ($h -match '(^|\W)try\s*$')
            $isType = $h -match $typeKw
            $isFunc = (-not $isControl) -and (-not $isType) -and ($h -match '\)\s*(const\s*|noexcept\s*|override\s*|final\s*|mutable\s*|->[^;{]*)*$')
            $parentCtrl = if ($stack.Count -gt 0) { $stack.Peek().ctrl } else { 0 }
            $kind = if ($isControl) { 'control' } elseif ($isFunc) { 'func' } else { 'struct' }
            $ctrl = if ($isControl) { $parentCtrl + 1 } else { $parentCtrl }
            if ($ctrl -gt $maxCtrl) { $maxCtrl = $ctrl; $maxCtrlLine = $line }
            $stack.Push([pscustomobject]@{ kind = $kind; line = $line; ctrl = $ctrl })
            $paren = 0; [void]$head.Clear()
        }
        elseif ($ch -eq '}') {
            if ($stack.Count -gt 0) {
                $top = $stack.Pop()
                if ($top.kind -eq 'func') { $funcs += [pscustomobject]@{ line = $top.line; len = ($line - $top.line + 1) } }
            }
            $paren = 0; [void]$head.Clear()
        }
        elseif ($ch -eq ';') { if ($paren -le 0) { [void]$head.Clear() } else { [void]$head.Append($ch) } }
        else { [void]$head.Append($ch) }
    }
    [pscustomobject]@{ maxCtrl = $maxCtrl; maxCtrlLine = $maxCtrlLine; funcs = $funcs }
}

$fails = 0; $warns = 0; $checked = 0
foreach ($f in $files) {
    $checked++
    $rel = $f.FullName.Substring($Root.Length).TrimStart('\', '/').Replace('\', '/')
    $raw = [System.IO.File]::ReadAllText($f.FullName, $utf8)
    $lineCount = ($raw -split "`r?`n").Count
    if ($raw.EndsWith("`n")) { $lineCount-- }
    $g = Measure-Geometry (Get-CleanText $raw)

    if ($g.maxCtrl -gt $NestingCap) {
        $fails++
        "FAIL  $rel : control-flow nesting depth $($g.maxCtrl) over the cap $NestingCap near line $($g.maxCtrlLine) - COD-6 hard cap; refactor (hoist a helper, invert with a guard, or dispatch)."
    }
    if ($lineCount -gt $FileSplit) {
        $warns++
        "warn  $rel : $lineCount lines over the split ceiling $FileSplit - COD-5; split at a named seam unless genuinely linear with a stated reason."
    }
    elseif ($lineCount -gt $FileSignal) {
        $warns++
        "warn  $rel : $lineCount lines over the signal ceiling $FileSignal - COD-5; consider factoring a unit out."
    }
    foreach ($fn in $g.funcs) {
        if ($fn.len -gt $FuncSplit) {
            $warns++
            "warn  $rel : a function at line $($fn.line) is $($fn.len) lines, over the split ceiling $FuncSplit - COD-5; split unless a stated reason says it is linear."
        }
        elseif ($fn.len -gt $FuncSignal) {
            $warns++
            "warn  $rel : a function at line $($fn.line) is $($fn.len) lines, over the signal ceiling $FuncSignal - COD-5; consider factoring."
        }
    }
}

''
"checked $checked source file(s): $fails nesting-cap failure(s), $warns COD-5 warning(s)."
if ($fails -gt 0) { 'RESULT: FAIL (COD-6 nesting cap).'; exit 1 }
if ($Strict -and $warns -gt 0) { 'RESULT: FAIL (-Strict: warnings gate).'; exit 1 }
'RESULT: PASS.'
exit 0
