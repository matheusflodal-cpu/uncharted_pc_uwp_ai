#requires -Version 7.2
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$true
$root=Split-Path $PSScriptRoot -Parent
$vswhere="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if(!(Test-Path $vswhere)) { throw 'Install Visual Studio with C++, Windows SDK and clang-cl first.' }
$vs=& $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs) { throw 'No Visual Studio C++ installation found.' }
& "$vs/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$env:PATH="$vs/VC/Tools/Llvm/x64/bin;$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin;$env:PATH"
if(!(Get-Command clang-cl -ErrorAction SilentlyContinue)) { throw 'Install the Visual Studio LLVM/Clang component.' }
python (Join-Path $PSScriptRoot 'fetch-deps.py')
if($LASTEXITCODE) { throw 'Dependency preparation failed.' }
$build=Join-Path $root '.local/build'
cmake -S $root -B $build -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=RelWithDebInfo
if($LASTEXITCODE) { throw 'CMake configuration failed.' }
cmake --build $build --parallel 4
if($LASTEXITCODE) { throw 'UWP compilation failed.' }
Write-Output 'Built the CPU diagnostic target. This is not a playable port.'
