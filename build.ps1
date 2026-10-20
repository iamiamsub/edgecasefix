# Builds edgecasefix.dll, tt_log.dll and ttlab.exe with MSVC and the vcpkg that ships with Visual Studio,
# and the release zip (edgecasefix.dll and dist-readme.txt as README.txt):
#
#   .\build.ps1        # -> build\edgecasefix.dll, build\tt_log.dll, build\ttlab.exe, dist\edgecasefix_<version>.zip
$ErrorActionPreference = "Stop"

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio with the C++ tools was not found" }
$vcpkg = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { Join-Path $vs "VC\vcpkg" }

Import-Module (Join-Path $vs "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null

Set-Location $PSScriptRoot

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

$version = (Select-String -Path CMakeLists.txt -Pattern "project\(edgecasefix VERSION ([0-9.]+)").Matches[0].Groups[1].Value
$stage = Join-Path dist "edgecasefix_$version"
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item build\edgecasefix.dll $stage -Force
Copy-Item dist-readme.txt (Join-Path $stage README.txt) -Force
Compress-Archive -Path "$stage\*" -DestinationPath "$stage.zip" -Force
Write-Output "PACKED: $stage.zip"
