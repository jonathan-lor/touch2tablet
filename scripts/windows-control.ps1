<# Send one JSON request to the current user's daemon. Examples:
   .\scripts\windows-control.ps1
   .\scripts\windows-control.ps1 -Request '{"op":"stop"}'
#>
param([string]$Request = '{"op":"get"}', [string]$Endpoint)
$ErrorActionPreference = 'Stop'
if (!$Endpoint) {
    $configPath = [Environment]::GetFolderPath('LocalApplicationData').Replace('\', '/')
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($configPath))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
    $Endpoint = 'touch2tablet-' + $hash.Substring(0, 24)
}
$pipe = [IO.Pipes.NamedPipeClientStream]::new('.', $Endpoint, [IO.Pipes.PipeDirection]::InOut, [IO.Pipes.PipeOptions]::Asynchronous)
try {
    $pipe.Connect(3000)
    $bytes = [Text.Encoding]::UTF8.GetBytes($Request + "`n")
    $pipe.Write($bytes, 0, $bytes.Length)
    $pipe.Flush()
    $reader = [IO.StreamReader]::new($pipe)
    $read = $reader.ReadLineAsync()
    if (!$read.Wait(3000)) { throw 'Daemon reply timed out.' }
    $read.Result
} finally { $pipe.Dispose() }
