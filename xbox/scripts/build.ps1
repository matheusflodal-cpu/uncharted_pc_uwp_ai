#requires -Version 7.2
[CmdletBinding()]
param([switch]$GraphicsProbe,[string]$DozenDirectory="",[string]$DxcDirectory="")
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
$dependencyArgs=@()
if($GraphicsProbe) { $dependencyArgs+='--graphics-probe' }
python (Join-Path $PSScriptRoot 'fetch-deps.py') @dependencyArgs
if($LASTEXITCODE) { throw 'Dependency preparation failed.' }
$build=Join-Path $root '.local/build'
$graphicsOption=if($GraphicsProbe) { 'ON' } else { 'OFF' }
cmake -S $root -B $build -G Ninja "-DUNCHARTED_GRAPHICS_PROBE=$graphicsOption" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=RelWithDebInfo
if($LASTEXITCODE) { throw 'CMake configuration failed.' }
cmake --build $build --parallel 4
if($LASTEXITCODE) { throw 'UWP compilation failed.' }
if($GraphicsProbe) {
    $layout=Join-Path $build 'layout'
    foreach($file in @('vulkan_dzn.dll','MIT')) {
        if(!(Test-Path (Join-Path $DozenDirectory $file))) { throw "Missing Dozen file $file" }
    }
    Copy-Item (Join-Path $DozenDirectory 'vulkan_dzn.dll') $layout -Force
    Copy-Item (Join-Path $DozenDirectory 'MIT') (Join-Path $layout 'ThirdParty/Mesa-MIT.txt') -Force
    foreach($file in @('dxcompiler.dll','dxil.dll')) {
        Copy-Item (Join-Path $DxcDirectory "bin/x64/$file") $layout -Force
    }
    Copy-Item (Join-Path $DxcDirectory 'LICENSE*') (Join-Path $layout 'ThirdParty') -Force
    Copy-Item (Join-Path $root '.local/deps/vulkan-headers/LICENSE*') (Join-Path $layout 'ThirdParty') -Force
    $manifest=Join-Path $layout 'AppxManifest.xml'
    (Get-Content $manifest -Raw).Replace('Version="0.1.0.0"','Version="0.2.0.0"').Replace('CPU and memory diagnostic;','Graphics bridge diagnostic;') | Set-Content $manifest -Encoding utf8NoBOM
}
Write-Output 'Built the diagnostic target. This is not a playable port.'
