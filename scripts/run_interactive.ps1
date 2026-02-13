param(
    [switch]$Debug,
    [string]$ExePath = ''
)

$workspaceRoot = Split-Path -Path $PSScriptRoot -Parent

if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $fixedCandidates = @(
        (Join-Path $workspaceRoot 'LLMapp\x64\Release\LLMapp.exe'),
        (Join-Path $workspaceRoot 'x64\Release\LLMapp.exe')
    )

    $ExePath = $fixedCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (-not $ExePath) {
        $found = Get-ChildItem -Path $workspaceRoot -Recurse -Filter 'LLMapp.exe' -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($found) {
            $ExePath = $found.FullName
        }
    }
}

if (-not $ExePath -or -not (Test-Path $ExePath)) {
    Write-Error 'LLMapp.exe was not found. Build first: .\scripts\build_and_run.ps1'
    exit 1
}

chcp 65001 > $null
[Console]::InputEncoding = New-Object System.Text.UTF8Encoding($false)
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$OutputEncoding = New-Object System.Text.UTF8Encoding($false)

Set-Location (Split-Path -Path $ExePath -Parent)

Write-Host ('Starting: ' + $ExePath)
Write-Host 'Type exit or quit to close.'

$launchArgs = @()
if ($Debug.IsPresent) {
    $launchArgs += '--debug'
}

& $ExePath @launchArgs
exit $LASTEXITCODE
