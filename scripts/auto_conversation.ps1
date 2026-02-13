param(
    [string]$ExePath = '',
    [string]$InputFile = '',
    [string[]]$Messages = @(),
    [switch]$Debug
)

$workspaceRoot = Split-Path -Path $PSScriptRoot -Parent

if ([string]::IsNullOrWhiteSpace($ExePath)) {
    $candidates = @(
        (Join-Path $workspaceRoot 'LLMapp\x64\Release\LLMapp.exe'),
        (Join-Path $workspaceRoot 'x64\Release\LLMapp.exe')
    )
    $ExePath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

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
    Write-Error 'LLMapp.exe が見つかりません。先にビルドしてください。'
    exit 1
}

$allInputs = New-Object System.Collections.Generic.List[string]

if (-not [string]::IsNullOrWhiteSpace($InputFile)) {
    if (-not (Test-Path $InputFile)) {
        Write-Error "InputFile が見つかりません: $InputFile"
        exit 1
    }

    $fileLines = Get-Content -Path $InputFile -Encoding UTF8 |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    foreach ($line in $fileLines) {
        $allInputs.Add($line)
    }
}

foreach ($msg in $Messages) {
    if (-not [string]::IsNullOrWhiteSpace($msg)) {
        $allInputs.Add($msg)
    }
}

if ($allInputs.Count -eq 0) {
    Write-Error '会話入力がありません。-Messages か -InputFile を指定してください。'
    exit 1
}

$allInputs.Add('exit')
$payload = ($allInputs -join "`r`n")

chcp 65001 > $null
[Console]::InputEncoding = New-Object System.Text.UTF8Encoding($false)
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$OutputEncoding = New-Object System.Text.UTF8Encoding($false)

$exeDir = Split-Path -Path $ExePath -Parent
Set-Location $exeDir

Write-Host ('Starting auto conversation: ' + $ExePath)
Write-Host ('Turns: ' + ($allInputs.Count - 1))

$args = @()
if ($Debug.IsPresent) {
    $args += '--debug'
}

$payload | & $ExePath @args
exit $LASTEXITCODE
