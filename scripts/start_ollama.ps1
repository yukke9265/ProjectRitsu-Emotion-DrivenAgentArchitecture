param(
    [string]$Model = "qwen3.5-9b-ud:q4kxl",
    [string]$Prompt = "",
    [int]$ServerWaitSeconds = 10,
    [bool]$EnsureGpu = $true,
    [bool]$AutoContext = $false,
    [int]$MinGpuRatioPercent = 70,
    [int]$MaxPredict = 256,
    [int]$SingleShotTimeoutSec = 180
)

$ErrorActionPreference = "Stop"

function Test-OllamaCommand {
    return [bool](Get-Command ollama -ErrorAction SilentlyContinue)
}

function Test-OllamaServer {
    try {
        ollama list *> $null
        return $true
    }
    catch {
        return $false
    }
}

function Stop-OllamaServerProcess {
    $serverProcesses = Get-CimInstance Win32_Process -Filter "Name='ollama.exe'" |
        Where-Object { $_.CommandLine -match "serve" }

    foreach ($process in $serverProcesses) {
        try {
            Stop-Process -Id $process.ProcessId -Force -ErrorAction Stop
        }
        catch {
        }
    }
}

function Set-OllamaGpuEnvironment {
    $env:OLLAMA_SCHED_SPREAD = "1"
    $env:OLLAMA_FLASH_ATTENTION = "1"
}

function Get-OllamaProcessorRatio {
    param(
        [string]$TargetModel
    )

    $psOutput = ollama ps | Out-String
    $lines = $psOutput -split "`r?`n"
    $targetLine = $lines | Where-Object { $_ -match [Regex]::Escape($TargetModel) } | Select-Object -First 1

    if (-not $targetLine) {
        return @{ Cpu = 0; Gpu = 0 }
    }

    $match = [regex]::Match($targetLine, "(\d+)%\/(\d+)%\s+CPU\/GPU")
    if (-not $match.Success) {
        return @{ Cpu = 0; Gpu = 0 }
    }

    return @{
        Cpu = [int]$match.Groups[1].Value
        Gpu = [int]$match.Groups[2].Value
    }
}

function Test-ContextCandidate {
    param(
        [string]$TargetModel,
        [int]$ContextLength,
        [int]$RequiredGpuRatio
    )

    $requestBody = @{
        model = $TargetModel
        prompt = "ctx-probe"
        stream = $false
        options = @{
            num_ctx = $ContextLength
            num_predict = 1
            temperature = 0
        }
    } | ConvertTo-Json -Depth 6

    try {
        $null = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:11434/api/generate" -ContentType "application/json" -Body $requestBody -TimeoutSec 180
    }
    catch {
        return $false
    }

    $ratio = Get-OllamaProcessorRatio -TargetModel $TargetModel
    return ($ratio.Gpu -ge $RequiredGpuRatio)
}

function Get-MaxSafeContext {
    param(
        [string]$TargetModel,
        [int]$RequiredGpuRatio
    )

    $candidates = @(4096, 8192, 12288, 16384, 24576, 32768, 49152, 65536)
    $best = 4096

    foreach ($candidate in $candidates) {
        Write-Host "コンテキスト探索: num_ctx=$candidate"
        $ok = Test-ContextCandidate -TargetModel $TargetModel -ContextLength $candidate -RequiredGpuRatio $RequiredGpuRatio
        if ($ok) {
            $best = $candidate
        }
        else {
            break
        }
    }

    return $best
}

function Invoke-OneShotInference {
    param(
        [string]$TargetModel,
        [string]$InputPrompt,
        [int]$PredictLimit,
        [int]$TimeoutSec
    )

    $numCtx = 4096
    if (-not [string]::IsNullOrWhiteSpace($env:OLLAMA_CONTEXT_LENGTH)) {
        [void][int]::TryParse($env:OLLAMA_CONTEXT_LENGTH, [ref]$numCtx)
    }

    $requestBody = @{
        model = $TargetModel
        prompt = $InputPrompt
        stream = $false
        options = @{
            num_ctx = $numCtx
            num_predict = $PredictLimit
        }
    } | ConvertTo-Json -Depth 6

    $response = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:11434/api/generate" -ContentType "application/json" -Body $requestBody -TimeoutSec $TimeoutSec
    if ($null -ne $response.response) {
        Write-Output $response.response
    }
}

if (-not (Test-OllamaCommand)) {
    throw "ollama コマンドが見つかりません。Ollama をインストールしてください。"
}

if ($EnsureGpu) {
    Set-OllamaGpuEnvironment
    Stop-OllamaServerProcess

    for ($i = 0; $i -lt $ServerWaitSeconds; $i++) {
        if (-not (Test-OllamaServer)) {
            break
        }
        Start-Sleep -Seconds 1
    }
}

if (-not (Test-OllamaServer)) {
    Write-Host "Ollama サーバーを起動します..."
    Start-Process -FilePath "ollama" -ArgumentList "serve" -WindowStyle Hidden | Out-Null

    $ready = $false
    for ($i = 0; $i -lt $ServerWaitSeconds; $i++) {
        Start-Sleep -Seconds 1
        if (Test-OllamaServer) {
            $ready = $true
            break
        }
    }

    if (-not $ready) {
        throw "Ollama サーバーの起動確認に失敗しました。"
    }
}

if ($EnsureGpu) {
    Write-Host "GPU優先設定を適用: OLLAMA_SCHED_SPREAD=$($env:OLLAMA_SCHED_SPREAD), OLLAMA_FLASH_ATTENTION=$($env:OLLAMA_FLASH_ATTENTION)"
}

if ($AutoContext) {
    $selectedContext = Get-MaxSafeContext -TargetModel $Model -RequiredGpuRatio $MinGpuRatioPercent
    $env:OLLAMA_CONTEXT_LENGTH = [string]$selectedContext
    Write-Host "自動設定コンテキスト: OLLAMA_CONTEXT_LENGTH=$selectedContext (GPU比率しきい値: $MinGpuRatioPercent%)"
}
else {
    $env:OLLAMA_CONTEXT_LENGTH = "24576"
    Write-Host "固定コンテキストを適用: OLLAMA_CONTEXT_LENGTH=$($env:OLLAMA_CONTEXT_LENGTH)"
}

if ([string]::IsNullOrWhiteSpace($Prompt)) {
    Write-Host "対話モードで起動: $Model"
    ollama run $Model
}
else {
    Write-Host "単発プロンプト実行: $Model"
    Invoke-OneShotInference -TargetModel $Model -InputPrompt $Prompt -PredictLimit $MaxPredict -TimeoutSec $SingleShotTimeoutSec
}
