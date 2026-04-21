param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [Parameter(Mandatory = $true)]
    [string]$WorkingDirectory,

    [int]$TimeoutSeconds = 20
)

$stdoutPath = Join-Path $WorkingDirectory 'cold_start_welcome_shell_stdout.log'
$stderrPath = Join-Path $WorkingDirectory 'cold_start_welcome_shell_stderr.log'
Remove-Item $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue

function Read-LogText {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path $Path)) {
        return ''
    }

    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite)
    try {
        $reader = New-Object System.IO.StreamReader($stream)
        try {
            return $reader.ReadToEnd()
        }
        finally {
            $reader.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

$process = Start-Process `
    -FilePath $ExePath `
    -WorkingDirectory $WorkingDirectory `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath `
    -PassThru

$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$combinedOutput = ''

try {
    while ([DateTime]::UtcNow -lt $deadline) {
        $combinedOutput = Read-LogText -Path $stdoutPath
        $stderrText = Read-LogText -Path $stderrPath
        if ($stderrText.Length -gt 0) {
            $combinedOutput += [Environment]::NewLine + $stderrText
        }

        if (
            $combinedOutput.Contains('[StartupShell] shown') -and
            $combinedOutput.Contains('[StartupShell] enter enabled') -and
            $combinedOutput.Contains('Executing phase: Startup complete')
        ) {
            break
        }

        if ($process.HasExited) {
            break
        }

        Start-Sleep -Milliseconds 250
    }

    $shellIndex = $combinedOutput.IndexOf('[StartupShell] shown')
    $enterEnabledIndex = $combinedOutput.IndexOf('[StartupShell] enter enabled')
    $startupCompleteIndex = $combinedOutput.IndexOf('Executing phase: Startup complete')

    if ($shellIndex -lt 0) {
        throw 'cold_start_welcome_shell_smoke_failed: Startup shell was not shown'
    }
    if ($enterEnabledIndex -lt 0) {
        throw 'cold_start_welcome_shell_smoke_failed: Enter System was not enabled'
    }
    if ($startupCompleteIndex -lt 0) {
        throw 'cold_start_welcome_shell_smoke_failed: Startup complete marker not observed'
    }
    if ($shellIndex -gt $startupCompleteIndex) {
        throw 'cold_start_welcome_shell_smoke_failed: Startup shell appeared after Startup complete'
    }
    if ($enterEnabledIndex -gt $startupCompleteIndex) {
        throw 'cold_start_welcome_shell_smoke_failed: Enter System enabled after Startup complete'
    }

    Write-Host 'Cold start welcome shell smoke passed'
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
    }
}
