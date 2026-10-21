# Isolate the shader compiler contract before rebuilding the full game runtime.
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$true
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& "$vs/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$env:PATH="$vs/VC/Tools/Llvm/x64/bin;$env:PATH"
python (Join-Path $PSScriptRoot 'fetch-deps.py') --graphics
if($LASTEXITCODE) { throw 'Shader contract dependency fetch failed.' }
$build=Join-Path $root 'xbox/.local/integer-contract'
New-Item -ItemType Directory $build -Force | Out-Null
$layout=Join-Path $build 'layout'
New-Item -ItemType Directory $layout -Force | Out-Null
Copy-Item "$env:RUNNER_TEMP/dozen/vulkan_dzn.dll" $layout -Force
Copy-Item "$env:RUNNER_TEMP/dxc/bin/x64/dxcompiler.dll","$env:RUNNER_TEMP/dxc/bin/x64/dxil.dll" $layout -Force
$sirit=Join-Path $root 'gpu/third_party/sirit'
$sources=@((Join-Path $root 'tests/test_uwp_integer_driver.cpp'),(Join-Path $root 'tests/test_uwp_integer_sample.cpp'),(Join-Path $sirit 'src/sirit.cpp'),(Join-Path $sirit 'src/stream.cpp'))
$sources+=Get-ChildItem "$sirit/src/instructions/*.cpp" | ForEach-Object FullName
Push-Location $build
try {
  & clang-cl /nologo /std:c++20 /O1 /EHsc /DUNCHARTED_INTEGER_TEST_NO_MAIN /DNOMINMAX /DWIN32_LEAN_AND_MEAN `
    "/I$root/gpu/shadps4" "/I$sirit/include" "/I$sirit/src" "/I$sirit/externals/SPIRV-Headers/include" "/I$root/xbox/.local/deps/vulkan-headers/include" `
    @sources /Feinteger-driver.exe /link kernel32.lib
  if($LASTEXITCODE) { throw 'Standalone integer shader build failed.' }
  # Diagnostic disassembly is confined to this host test, never the package.
  $env:DZN_DEBUG='dxil'
  & ./integer-driver.exe $layout
  if($LASTEXITCODE) { throw 'Standalone Dozen integer shader contract failed.' }
} finally { Remove-Item Env:DZN_DEBUG -ErrorAction SilentlyContinue;Pop-Location }
