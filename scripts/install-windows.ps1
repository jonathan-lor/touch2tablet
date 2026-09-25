#Requires -RunAsAdministrator
<# Install a release build with T2T_WINDOWS_UIACCESS=ON, staged by windeployqt.
   Production: supply a signed daemon. Local development: optionally sign with an
   already trusted code-signing certificate. This script never creates trust.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Stage,
    [string]$CertificateThumbprint,
    [string]$LogPath
)
$ErrorActionPreference = 'Stop'
if ($LogPath) { Start-Transcript -LiteralPath $LogPath }
try {
    $stagePath = (Resolve-Path -LiteralPath $Stage).Path
    $installPath = Join-Path $env:ProgramFiles 'touch2tablet'
    if ($stagePath -eq $installPath -or $stagePath.StartsWith($installPath + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Stage the build outside the installation directory.'
    }
    if (Test-Path -LiteralPath $installPath) {
        $existing = @(Get-Item -LiteralPath $installPath) + @(Get-ChildItem -LiteralPath $installPath -Recurse -Force)
        foreach ($item in $existing) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Refusing to overwrite an installation containing links.'
            }
        }
    }
    foreach ($name in @('touch2tabletd.exe', 'touch2tablet.exe', 'Qt6Core.dll', 'Qt6Network.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms\qwindows.dll')) {
        if (!(Test-Path -LiteralPath (Join-Path $stagePath $name) -PathType Leaf)) { throw "Missing staged file: $name" }
    }
    foreach ($process in Get-Process -Name touch2tablet,touch2tabletd -ErrorAction SilentlyContinue) {
        if ($process.Path -and [IO.Path]::GetDirectoryName($process.Path) -eq $installPath) {
            throw 'Exit touch2tablet and stop its daemon before updating the installation.'
        }
    }
    # Validate the staged daemon before modifying a working installation.
    $daemon = Join-Path $stagePath 'touch2tabletd.exe'
    if ($CertificateThumbprint) {
        if ($CertificateThumbprint -notmatch '^[0-9A-Fa-f]{40}$') { throw 'Invalid certificate thumbprint.' }
        $certificate = Get-Item -LiteralPath "Cert:\LocalMachine\My\$CertificateThumbprint"
        if (!$certificate.HasPrivateKey -or $certificate.NotAfter -lt (Get-Date)) { throw 'Signing certificate unavailable or expired.' }
        Set-AuthenticodeSignature -LiteralPath $daemon -Certificate $certificate -HashAlgorithm SHA256 | Out-Null
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $daemon
    if ($signature.Status -ne 'Valid') { throw "Daemon signature is not trusted: $($signature.StatusMessage)" }
    New-Item -ItemType Directory -Path $installPath -Force | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $stagePath) {
        Copy-Item -LiteralPath $item.FullName -Destination $installPath -Recurse -Force
    }
    $record = [ordered]@{ installPath = $installPath; certificateThumbprint = $signature.SignerCertificate.Thumbprint }
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $installPath 'installation.json')
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'touch2tablet.lnk'))
    $shortcut.TargetPath = Join-Path $installPath 'touch2tablet.exe'
    $shortcut.WorkingDirectory = $installPath
    $shortcut.Description = 'Configure touch panel tablet mapping'
    $shortcut.Save()
    Write-Output "Installed touch2tablet at $installPath. Open it from the Start menu."
    Write-Output 'Start with Windows is optional in the notification-area menu. Settings are preserved across updates.'
} finally {
    if ($LogPath) { Stop-Transcript }
}
