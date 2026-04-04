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

function Sync-LlamaRuntimeDlls {
    param(
        [string]$WorkspaceRoot,
        [string]$BuildConfiguration,
        [string]$BuildPlatform
    )

    $sourceCandidates = @(
        (Join-Path $WorkspaceRoot ("external\llama.cpp\build\bin\{0}" -f $BuildConfiguration)),
        (Join-Path $WorkspaceRoot "external\llama.cpp\build\bin\Release"),
        (Join-Path $WorkspaceRoot "external\llama.cpp\build\bin\Debug")
    )

    $sourceDir = $sourceCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $sourceDir) {
        Write-Warning "llama.cpp runtime DLL directory was not found under external\\llama.cpp\\build\\bin."
        return
    }

    $targetDir = Join-Path $WorkspaceRoot ("LLMapp\\{0}\\{1}" -f $BuildPlatform, $BuildConfiguration)
    if (-not (Test-Path $targetDir)) {
        Write-Warning "Target output directory does not exist yet: $targetDir"
        return
    }

    $dlls = Get-ChildItem -Path $sourceDir -Filter *.dll -File -ErrorAction SilentlyContinue
    if (-not $dlls) {
        Write-Warning "No runtime DLLs found in $sourceDir"
        return
    }

    foreach ($dll in $dlls) {
        Copy-Item -Path $dll.FullName -Destination (Join-Path $targetDir $dll.Name) -Force
    }

    Write-Host "Synced llama.cpp runtime DLLs from $sourceDir to $targetDir"
}

Write-Host "Building $solutionPath (Configuration=$Configuration, Platform=$Platform) using '$MSBuildPath'"
& $MSBuildPath $solutionPath /t:Build /p:Configuration=$Configuration;Platform=$Platform
if ($LASTEXITCODE -ne 0) {
    Write-Error "MSBuild failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Sync-LlamaRuntimeDlls -WorkspaceRoot $scriptDir -BuildConfiguration $Configuration -BuildPlatform $Platform

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
