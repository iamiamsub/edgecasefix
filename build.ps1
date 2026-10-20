# Builds edgecasefix.dll, tt_log.dll and ttlab.exe with MSVC and vcpkg, and the release zip
# (edgecasefix.dll, README.txt from dist-readme.txt, LICENSE.txt):
#
#   .\build.ps1                  # version = the project VERSION in CMakeLists.txt
#   .\build.ps1 -Version 1.0.1   # -> build\*.dll, build\ttlab.exe, dist\edgecasefix-1.0.1.zip
#
# vcpkg is the one in VCPKG_ROOT when it is set (the GitHub workflow clones its own), else the one
# that ships with Visual Studio (C++ workload). The release is leak scanned when build.local.ps1
# (not in the repository) sets the scanner:
#
#   $leakScan = "<path to a script that exits non-zero when it finds personal data>"
param([string]$Version)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (-not $Version) {
    $Version = (Select-String -Path CMakeLists.txt -Pattern "project\(edgecasefix VERSION ([0-9.]+)").Matches[0].Groups[1].Value
}

$leakScan = $null
if (Test-Path build.local.ps1) { . .\build.local.ps1 }
if (-not $leakScan) { Write-Warning "no leak scanner set in build.local.ps1: the release is not scanned" }

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio with the C++ tools was not found" }

# Read before entering the Visual Studio shell, which may set VCPKG_ROOT to its own copy.
$vcpkg = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { Join-Path $vs "VC\vcpkg" }

Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null

cmake -B build -S . -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static `
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
    -DCMAKE_TOOLCHAIN_FILE="$vcpkg\scripts\buildsystems\vcpkg.cmake"
if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }

cmake --build build
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

foreach ($f in "edgecasefix.dll", "tt_log.dll", "ttlab.exe") {
    $item = Get-Item (Join-Path build $f)
    Write-Output "BUILT: $f ($([math]::Round($item.Length / 1kb)) KB)"
}

if (Test-Path dist) { Remove-Item -Recurse -Force dist }
$out = Join-Path $PSScriptRoot "dist\edgecasefix-$Version"   # absolute: .NET calls ignore Set-Location
New-Item -ItemType Directory $out | Out-Null
Copy-Item build\edgecasefix.dll $out
Copy-Item dist-readme.txt (Join-Path $out README.txt)
Copy-Item LICENSE (Join-Path $out LICENSE.txt)

# Nothing personal may leave this PC: stop here if the scan finds the user name or a home path.
if ($leakScan) {
    python $leakScan dist
    if ($LASTEXITCODE -ne 0) { throw "leak scan found personal data - release stopped" }
}

Compress-Archive -Path "$out\*" -DestinationPath "$out.zip"
if ($leakScan) {
    python $leakScan "$out.zip"
    if ($LASTEXITCODE -ne 0) { throw "leak scan found personal data in the zip - release stopped" }
}
Write-Output "PACKED: dist\edgecasefix-$Version.zip"
