# Runs every scenario on the game's own turntable state machine (ttlab.exe) and compares, frame by
# frame:
#   - 012 + edgecasefix.dll at TDJ 120 fps against 010 at TDJ 120 fps: must be the same (the fix)
#   - 012 at TDJ 120 fps against 010: how many frames differ (the bug)
#   - 012 + edgecasefix.dll at LDJ 60 fps against 012 alone: must be the same (LDJ untouched)
#   - 010 + edgecasefix.dll against 010 alone: must be the same (nothing to change there)
#
# The game libraries are your own copies (not in this repository); each build is named by its folder.
#
#   .\run.ps1 -Reference <010>\bm2dx.dll -Builds <012>\bm2dx.dll, <012>\bm2dx.dll   # all scenarios; traces in lab\out
#   .\run.ps1 ... -Scenario bss_reverse_drift -Show   # one, with the presses and stops of each run
param(
    [Parameter(Mandatory)] [string]$Reference,  # an 010 build's bm2dx.dll: the turntable as it should be
    [Parameter(Mandatory)] [string[]]$Builds,   # 012 builds' bm2dx.dll
    [string]$Scenario = "*",
    [switch]$Show
)
$ErrorActionPreference = "Stop"

$bin = Join-Path $PSScriptRoot "..\build"
$out = Join-Path $PSScriptRoot "out"
New-Item -ItemType Directory -Force $out | Out-Null

$ref = $Reference
$games = [ordered]@{}
foreach ($b in $Builds) { $games[(Split-Path (Split-Path $b -Parent) -Leaf)] = $b }
$fix = Join-Path $bin "edgecasefix.dll"

function Run($game, $scenario, $name, [string[]]$extra) {
    $trace = Join-Path $out "$name.txt"
    $text = & (Join-Path $bin "ttlab.exe") $game $scenario --out $trace @extra 2>&1
    if ($LASTEXITCODE -ne 0) { throw "ttlab failed: $name`n$text" }
    if ($Show) { Write-Host "  [$name]"; $text | ForEach-Object { Write-Host "    $_" } }
    return $trace
}

function Differ($a, $b) {
    $x = Get-Content $a; $y = Get-Content $b
    if ($x.Count -ne $y.Count) { return [math]::Max($x.Count, $y.Count) }
    return @(0..($x.Count - 1) | Where-Object { $x[$_] -ne $y[$_] }).Count
}

$failed = 0
foreach ($s in Get-ChildItem (Join-Path $PSScriptRoot "scenarios") -Filter "$Scenario.txt") {
    $n = $s.BaseName
    "== $n"
    $r = Run $ref $s.FullName "$n.010" @("--io", "3", "--fps", "120")
    $rf = Run $ref $s.FullName "$n.010+fix" @("--io", "3", "--fps", "120", "-k", $fix)
    $d = Differ $r $rf
    "   010 + edgecasefix     vs 010 : $(if ($d) { "DIFFER ($d frames)  FAIL" } else { 'same  ok' })"
    if ($d) { $failed++ }

    foreach ($b in $games.Keys) {
        $raw = Run $games[$b] $s.FullName "$n.$b" @("--io", "3", "--fps", "120")
        $fixed = Run $games[$b] $s.FullName "$n.$b+fix" @("--io", "3", "--fps", "120", "-k", $fix)
        $ldj = Run $games[$b] $s.FullName "$n.$b.ldj" @("--io", "2", "--fps", "60")
        $ldjfix = Run $games[$b] $s.FullName "$n.$b.ldj+fix" @("--io", "2", "--fps", "60", "-k", $fix)
        $bug = Differ $r $raw
        $d1 = Differ $r $fixed
        $d2 = Differ $ldj $ldjfix
        "   $b TDJ                 vs 010 : $(if ($bug) { "$bug frames differ" } else { 'same' })"
        "   $b TDJ + edgecasefix   vs 010 : $(if ($d1) { "DIFFER ($d1 frames)  FAIL" } else { 'same  ok' })"
        "   $b LDJ + edgecasefix   vs LDJ : $(if ($d2) { "DIFFER ($d2 frames)  FAIL" } else { 'same  ok' })"
        if ($d1) { $failed++ }
        if ($d2) { $failed++ }
    }
}
""
if ($failed) { "NG: $failed comparison(s) failed" } else { "OK: every comparison as expected" }
exit $failed
