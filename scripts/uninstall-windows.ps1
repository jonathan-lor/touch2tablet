#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$expected = [IO.Path]::GetFullPath((Join-Path $env:ProgramFiles 'touch2tablet'))
if (Test-Path -LiteralPath $expected) {
    $resolved = (Resolve-Path -LiteralPath $expected).Path
    if ($resolved -ne $expected) { throw 'Unexpected installation path.' }
    $record = Get-Content -LiteralPath (Join-Path $resolved 'installation.json') -Raw | ConvertFrom-Json
    if ($record.installPath -ne $expected) { throw 'Installation record does not match.' }
    if ((Get-Item -LiteralPath $resolved).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Refusing to remove a redirected directory.' }
    foreach ($item in Get-ChildItem -LiteralPath $resolved -Recurse -Force) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Refusing to remove an installation containing links.' }
    }
    foreach ($process in Get-Process -Name touch2tablet,touch2tabletd -ErrorAction SilentlyContinue) {
        if ($process.Path -and [IO.Path]::GetDirectoryName($process.Path) -eq $expected) {
            throw 'Use Exit and restore touchscreen before uninstalling.'
        }
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
$shortcut = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'touch2tablet.lnk'
if (Test-Path -LiteralPath $shortcut) { Remove-Item -LiteralPath $shortcut }
Write-Output 'Application removed. User settings, presets, and signing certificates were preserved.'
