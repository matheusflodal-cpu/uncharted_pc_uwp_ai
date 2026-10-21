#requires -Version 7.2
[CmdletBinding()]
param([switch]$CompileRenderer,[switch]$GraphicsProbe,[string]$DozenDirectory="",[string]$DxcDirectory="")
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
if($CompileRenderer) {
    & (Join-Path $PSScriptRoot 'build-deps.ps1')
    if($LASTEXITCODE) { throw 'Renderer dependency preparation failed.' }
}
$dependencyArgs=@()
if($CompileRenderer) { $dependencyArgs+='--graphics' }
elseif($GraphicsProbe) { $dependencyArgs+='--graphics-probe' }
python (Join-Path $PSScriptRoot 'fetch-deps.py') @dependencyArgs
if($LASTEXITCODE) { throw 'Dependency preparation failed.' }
$build=Join-Path $root '.local/build'
$rendererOption=if($CompileRenderer) { 'ON' } else { 'OFF' }
$graphicsOption=if($GraphicsProbe) { 'ON' } else { 'OFF' }
cmake -S $root -B $build -G Ninja "-DUNCHARTED_GRAPHICS_PROBE=$graphicsOption" "-DUNCHARTED_COMPILE_RENDERER=$rendererOption" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=RelWithDebInfo
if($LASTEXITCODE) { throw 'CMake configuration failed.' }
if($CompileRenderer) {
    # Collect independent compilation errors in one run; Ninja still returns failure.
    cmake --build $build --parallel 4 -- -k 0
} else {
    cmake --build $build --parallel 4
}
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
    if($CompileRenderer) {
        $notices=Join-Path $layout 'ThirdParty'
        Copy-Item (Join-Path $root '../third_party/LibAtrac9/LICENSE') (Join-Path $notices 'LibAtrac9-LICENSE.txt') -Force
        $sources=Get-Content (Join-Path $PSScriptRoot 'graphics-dependencies.json') -Raw | ConvertFrom-Json -AsHashtable
        foreach($name in $sources.Keys) {
            Copy-Item (Join-Path $root ".local/deps/$name/$($sources[$name][2])") (Join-Path $notices "$name-LICENSE.txt") -Force
        }
        New-Item -ItemType Directory (Join-Path $notices 'Boost') -Force | Out-Null
        Copy-Item (Join-Path $root '.local/deps/boost/licenses/*') (Join-Path $notices 'Boost') -Force
        Copy-Item (Join-Path $root '../gpu/third_party/half/include/half.hpp') (Join-Path $notices 'half.hpp') -Force
        foreach($license in @('fonts/LICENSE-DejaVu.txt','sirit/LICENSE.txt','sirit/externals/SPIRV-Headers/LICENSE')) {
            Copy-Item (Join-Path $root "../gpu/third_party/$license") (Join-Path $notices ($license.Replace('/','-'))) -Force
        }
        foreach($spec in @(@('imgui','LICENSE.txt'),@('magic_enum','LICENSE'),@('miniz','LICENSE'),@('xbyak','COPYRIGHT'))) {
            Copy-Item (Join-Path $root "../gpu/third_party/$($spec[0])/$($spec[1])") (Join-Path $notices "$($spec[0])-LICENSE.txt") -Force
        }
    }
    $manifest=Join-Path $layout 'AppxManifest.xml'
    $packageVersion=if($CompileRenderer) { '0.6.5.0' } else { '0.2.0.0' }
    (Get-Content $manifest -Raw).Replace('Version="0.1.0.0"',"Version=`"$packageVersion`"").Replace('CPU and memory diagnostic;','Graphics bridge diagnostic;') | Set-Content $manifest -Encoding utf8NoBOM
}
Write-Output 'Built the diagnostic target. This is not a playable port.'
