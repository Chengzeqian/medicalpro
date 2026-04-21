param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,

    [Parameter(Mandatory = $true)]
    [string]$WorkingDirectory,

    [int]$TimeoutSeconds = 20,

    [int]$PollIntervalMilliseconds = 250
)

$stdoutPath = Join-Path $WorkingDirectory 'user_management_startup_stdout.log'
$stderrPath = Join-Path $WorkingDirectory 'user_management_startup_stderr.log'

Remove-Item $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue

function Read-LogText {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path $Path)) {
        return ''
    }

    $stream = $null
    $reader = $null

    try {
        $stream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite)
        $reader = New-Object System.IO.StreamReader($stream)
        return $reader.ReadToEnd()
    }
    catch {
        return ''
    }
    finally {
        if ($reader) {
            $reader.Dispose()
        }

        if ($stream) {
            $stream.Dispose()
        }
    }
}

$process = Start-Process `
    -FilePath $ExePath `
    -WorkingDirectory $WorkingDirectory `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath `
    -PassThru

$failureDetectors = @(
    @{
        Pattern = 'Plugin handle not found for UserManagement'
        Message = 'user_management_startup_smoke_failed: Plugin handle not found for UserManagement'
    },
    @{
        Pattern = 'Critical plugin start failed:\s+"?(org\.medicalpro\.user_management|UserManagement)"?'
        Message = 'user_management_startup_smoke_failed: Critical plugin start failed for UserManagement'
    },
    @{
        Pattern = 'Service ready timeout:\s+"?org\.medicalpro\.user_management"?'
        Message = 'user_management_startup_smoke_failed: Service ready timeout for org.medicalpro.user_management'
    }
)

$successPattern = 'Executing phase: Startup complete'
$combinedOutput = ''
$successObserved = $false
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)

try {
    while ([DateTime]::UtcNow -lt $deadline) {
        $combinedOutput = ''

        $stdoutText = Read-LogText -Path $stdoutPath
        $stderrText = Read-LogText -Path $stderrPath

        if ($stdoutText.Length -gt 0) {
            $combinedOutput += $stdoutText
        }

        if ($stderrText.Length -gt 0) {
            if ($combinedOutput.Length -gt 0) {
                $combinedOutput += [Environment]::NewLine
            }
            $combinedOutput += $stderrText
        }

        foreach ($failureDetector in $failureDetectors) {
            if ($combinedOutput -match $failureDetector.Pattern) {
                throw $failureDetector.Message
            }
        }

        if ($combinedOutput -match $successPattern) {
            $successObserved = $true
            break
        }

        if ($process.HasExited) {
            break
        }

        Start-Sleep -Milliseconds $PollIntervalMilliseconds
    }

    if (-not $successObserved) {
        throw "user_management_startup_smoke_timeout: Startup complete marker not observed within $TimeoutSeconds seconds"
    }

    Write-Host 'UserManagement startup smoke passed'
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
    }
}
