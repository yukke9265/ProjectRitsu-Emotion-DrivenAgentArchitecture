param(
    [string]$Solution = "LLMapp\LLMapp.sln",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$MSBuildPath = ""
)

$workspaceRoot = Split-Path -Path $PSScriptRoot -Parent
$solutionPath = Join-Path $workspaceRoot $Solution

if (-not (Test-Path $solutionPath)) {
    Write-Error "Solution not found: $solutionPath"
    exit 1
}

function Resolve-MSBuildPath {
    param([string]$Preferred)

    if (-not [string]::IsNullOrWhiteSpace($Preferred)) {
        if (Test-Path $Preferred) {
            return $Preferred
        }
        Write-Error "指定されたMSBuildPathが見つかりません: $Preferred"
        exit 1
    }

    $msbuildCmd = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($msbuildCmd) {
        return $msbuildCmd.Source
    }

    $vswhere = Join-Path "${env:ProgramFiles(x86)}" "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $resolved = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if (-not [string]::IsNullOrWhiteSpace($resolved)) {
            return $resolved.Trim()
        }
    }

    Write-Error "MSBuild.exe が見つかりません。Visual Studio Build Tools か Visual Studio をインストールしてください。"
    exit 1
}

$resolvedMSBuild = Resolve-MSBuildPath -Preferred $MSBuildPath

Write-Host "Building (minimal): $solutionPath"
Write-Host "Configuration=$Configuration Platform=$Platform"
Write-Host "MSBuild=$resolvedMSBuild"

& $resolvedMSBuild $solutionPath /t:Build /p:Configuration=$Configuration /p:Platform=$Platform /v:minimal /nologo
$exitCode = $LASTEXITCODE

if ($exitCode -eq 0) {
    Write-Host "Build succeeded." -ForegroundColor Green
} else {
    Write-Host "Build failed (exit code: $exitCode)." -ForegroundColor Red
}

exit $exitCode
