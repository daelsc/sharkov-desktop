param(
    [Parameter(Mandatory=$true)][string]$ExePath,
    [int]$WaitSeconds = 15
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $ExePath)) {
    throw "Executable not found: $ExePath"
}

Write-Host "Smoke test: launching $ExePath"
$proc = Start-Process -FilePath $ExePath -ArgumentList '--disable-gpu' -PassThru

Write-Host "Started PID $($proc.Id); waiting $WaitSeconds seconds for early crash..."

for ($i = 1; $i -le $WaitSeconds; $i++) {
    Start-Sleep -Seconds 1
    if ($proc.HasExited) {
        Write-Host "FAIL: process exited after $i second(s) with code $($proc.ExitCode)"
        exit 1
    }
}

Write-Host "PASS: app still running after $WaitSeconds seconds"
try {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    $proc.WaitForExit(5000) | Out-Null
} catch {
    Write-Host "Warning: failed to stop process cleanly: $_"
}

exit 0
