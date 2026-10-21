#requires -Version 7.2
[CmdletBinding()]
param([string]$Out='',[string]$CertificatePath='',[string]$CertificatePassword='')
$ErrorActionPreference='Stop'
$PSNativeCommandUseErrorActionPreference=$true
$root=Split-Path $PSScriptRoot -Parent
$layout=Join-Path $root '.local/build/layout'
if(!(Test-Path (Join-Path $layout 'uncharted_uwp.exe'))) { throw 'Build the diagnostic target first.' }
if(!$Out) { $Out=Join-Path $root '.local/packages' }
New-Item -ItemType Directory -Force $Out | Out-Null
$Out=[IO.Path]::GetFullPath($Out)
& (Join-Path $PSScriptRoot 'check-imports.ps1') -Folder $layout
& (Join-Path $PSScriptRoot 'assets.ps1')
$vs=& "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -property installationPath
& "$vs/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
$sdk=Join-Path $env:WindowsSdkVerBinPath 'x64'
$package=Join-Path $Out 'UnchartedUwpProbe_0.1.0.0_x64.appx'
& "$sdk/makeappx.exe" pack /d $layout /p $package /o
if($LASTEXITCODE) { throw 'Package creation failed.' }
if(!$CertificatePath) {
    # Generate a separate identity; never reuse the GoW3 publisher or overwrite its app.
    $cert=New-SelfSignedCertificate -Type Custom -Subject 'CN=UnchartedUwpProbe' -FriendlyName 'Uncharted UWP diagnostic test' `
        -KeyUsage DigitalSignature -CertStoreLocation 'Cert:\CurrentUser\My' `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}') -NotAfter (Get-Date).AddYears(1)
    try {
        Export-Certificate -Cert $cert -FilePath (Join-Path $Out 'UnchartedUwpProbe.cer') | Out-Null
        & "$sdk/signtool.exe" sign /fd SHA256 /sha1 $cert.Thumbprint /s My $package
        if($LASTEXITCODE) { throw 'Signing failed.' }
    } finally { Remove-Item "Cert:\CurrentUser\My\$($cert.Thumbprint)" -ErrorAction SilentlyContinue }
} else {
    & "$sdk/signtool.exe" sign /fd SHA256 /f $CertificatePath /p $CertificatePassword $package
    if($LASTEXITCODE) { throw 'Signing failed.' }
}
$vclibs="${env:ProgramFiles(x86)}\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs\14.0\Appx\Retail\x64\Microsoft.VCLibs.x64.14.00.appx"
if(!(Test-Path $vclibs)) { throw "VCLibs framework not found: $vclibs. Install the UWP components." }
New-Item -ItemType Directory -Force (Join-Path $Out 'Dependencies') | Out-Null
Copy-Item $vclibs (Join-Path $Out 'Dependencies') -Force
Copy-Item (Join-Path $root 'README.md') (Join-Path $Out 'README.md') -Force
Compress-Archive -Path $package,(Join-Path $Out 'Dependencies'),(Join-Path $Out 'README.md') `
    -DestinationPath (Join-Path $Out 'UnchartedUwpProbe-0.1.0.0.zip') -Force
Write-Output "Diagnostic package: $package"
