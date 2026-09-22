<#
.SYNOPSIS
    يوقّع GameLauncher.exe والمثبّت رقمياً (Authenticode) بـ signtool.

.DESCRIPTION
    يدعم ثلاث طرق (اختر واحدة):
      1) ملف PFX:                 -PfxPath cert.pfx -PfxPassword (Read-Host -AsSecureString)
      2) شهادة في مخزن ويندوز أو على توكن/HSM سحابي (مثل SimplySign):  -Thumbprint <SHA1>
      3) Azure Artifact Signing (لمن هو في دولة مدعومة):  -ArtifactDlib <المسار> -ArtifactMetadata <metadata.json>

    الترتيب الصحيح لإصدار:
      1. ابنِ Release          -> bin\Release\GameLauncher.exe
      2. وقّع الـ exe:         .\tools\sign-release.ps1 -Thumbprint XXXX -Files bin\Release\GameLauncher.exe
      3. ابنِ المثبّت (Inno)   -> dist\GameLauncher-Setup-<v>.exe   (يضم الـ exe الموقّع)
      4. وقّع المثبّت:         .\tools\sign-release.ps1 -Thumbprint XXXX -Files dist\GameLauncher-Setup-1.8.0.exe

    لم يُختبر هذا السكربت على شهادة حقيقية (لا أملك واحدة)؛ راجع docs\PUBLISHING.ar.md.
#>
param(
    [string[]]$Files = @(),
    [string]$PfxPath,
    [System.Security.SecureString]$PfxPassword,
    [string]$Thumbprint,
    [string]$ArtifactDlib,
    [string]$ArtifactMetadata,
    [string]$TimestampUrl = 'http://timestamp.digicert.com',
    [string]$Description = 'GameLauncher',
    [string]$SignToolPath,
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'

if (-not $Files -or $Files.Count -eq 0) {
    $Files = @('bin\Release\GameLauncher.exe') + @(Get-ChildItem 'dist\GameLauncher-Setup-*.exe' -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
}

if (-not $SignToolPath) {
    $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $found = Get-ChildItem $kits -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
             Where-Object { $_.FullName -match '\\x64\\' } | Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $found) { throw 'signtool.exe not found. Install the Windows SDK (Signing Tools) or pass -SignToolPath.' }
    $SignToolPath = $found.FullName
}
Write-Host "signtool: $SignToolPath"

foreach ($f in $Files) {
    if (-not (Test-Path $f)) { throw "File not found: $f" }
    $full = (Resolve-Path $f).Path
    if (-not $VerifyOnly) {
        $signArgs = @('sign', '/fd', 'SHA256', '/tr', $TimestampUrl, '/td', 'SHA256', '/d', $Description)
        if ($ArtifactDlib) {
            $signArgs += @('/dlib', $ArtifactDlib, '/dmdf', $ArtifactMetadata)
        }
        elseif ($PfxPath) {
            $plain = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($PfxPassword))
            $signArgs += @('/f', $PfxPath, '/p', $plain)
        }
        elseif ($Thumbprint) {
            $signArgs += @('/sha1', $Thumbprint)
        }
        else { throw 'Choose one: -PfxPath / -Thumbprint / -ArtifactDlib' }
        $signArgs += $full
        Write-Host "Signing $full ..." -ForegroundColor Cyan
        & $SignToolPath @signArgs
        if ($LASTEXITCODE -ne 0) { throw "signtool sign failed ($LASTEXITCODE)" }
    }
    Write-Host "Verifying $full ..." -ForegroundColor Cyan
    & $SignToolPath verify /pa /v $full
    if ($LASTEXITCODE -ne 0) { throw "signtool verify failed ($LASTEXITCODE)" }
}
Write-Host 'Done.' -ForegroundColor Green
