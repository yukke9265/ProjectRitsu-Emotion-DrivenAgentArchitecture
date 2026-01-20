param(
    [string]$Solution = "LLMapp.sln",
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$MSBuildPath = "msbuild"
)

$scriptDir = Split-Path -Path $PSScriptRoot -Parent
$solutionPath = Join-Path $scriptDir $Solution

# If the provided solution path doesn't exist, try common locations for LLMapp.sln
if (-not (Test-Path $solutionPath)) {
    $candidate = Join-Path $scriptDir 'LLMapp\LLMapp.sln'
    if (Test-Path $candidate) {
        $solutionPath = $candidate
    } else {
        $found = Get-ChildItem -Path $scriptDir -Recurse -Filter 'LLMapp.sln' -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) {
            $solutionPath = $found.FullName
        }
    }
}

if (-not (Test-Path $solutionPath)) {
    Write-Error "Solution not found: $solutionPath"
    exit 1
}

Write-Host "Building $solutionPath (Configuration=$Configuration, Platform=$Platform) using '$MSBuildPath'"
& $MSBuildPath $solutionPath /t:Build /p:Configuration=$Configuration;Platform=$Platform
if ($LASTEXITCODE -ne 0) {
    Write-Error "MSBuild failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# Try to find the built executable. Prefer LLMapp.exe, otherwise pick the newest .exe under the repo.
$exeName = "LLMapp.exe"
$workspaceRoot = $scriptDir
$exe = Get-ChildItem -Path $workspaceRoot -Recurse -Filter $exeName -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $exe) {
    $exe = Get-ChildItem -Path $workspaceRoot -Recurse -Filter *.exe -ErrorAction SilentlyContinue | Where-Object { $_.FullName -notmatch "\\packages\\" } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
}

if (-not $exe) {
    Write-Error "No executable found after build. You can pass -Solution to point to a different .sln file."
    exit 1
}

Write-Host "Running: $($exe.FullName)"
Start-Process -FilePath $exe.FullName -NoNewWindow -Wait
