param(
	[string]$ModelsDir = ".\models"
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command ollama -ErrorAction SilentlyContinue)) {
	throw "ollama コマンドが見つかりません。先に Ollama をインストールしてください。"
}

$resolvedModelsDir = (Resolve-Path $ModelsDir).Path
$ggufFiles = Get-ChildItem -Path $resolvedModelsDir -File -Filter *.gguf

if (-not $ggufFiles -or $ggufFiles.Count -eq 0) {
	throw "GGUFファイルが見つかりません: $resolvedModelsDir"
}

$nameMap = @{
	"gemma-2-9b-it-Q5_K_M.gguf" = "gemma2-9b:q5km"
	"LFM2.5-1.2B-Instruct-BF16.gguf" = "lfm2.5-1.2b:bf16"
	"Llama-3.1-Swallow-8B-Instruct-v0.3.Q6_K.gguf" = "llama3.1-swallow-8b:q6k"
	"NVIDIA-Nemotron-Nano-9B-v2-Japanese-Q4_K_M.gguf" = "nemotron-nano-9b-ja:q4km"
	"Qwen3.5-9B-Q4_K_M.gguf" = "qwen3.5-9b:q4km"
	"Qwen3.5-9B-UD-Q4_K_XL.gguf" = "qwen3.5-9b-ud:q4kxl"
	"Qwen_Qwen3-4B-Instruct-2507-Q5_K_M.gguf" = "qwen3-4b-instruct:q5km"
}

$created = @()

foreach ($file in $ggufFiles) {
	$tag = $nameMap[$file.Name]
	if (-not $tag) {
		$base = [System.IO.Path]::GetFileNameWithoutExtension($file.Name).ToLower()
		$base = $base -replace "[^a-z0-9._-]", "-"
		$tag = "$base:local"
	}

	$tmpModelfile = [System.IO.Path]::GetTempFileName()
	Set-Content -Path $tmpModelfile -Encoding utf8 -Value "FROM $($file.FullName)"

	try {
		Write-Host "[CREATE] $tag <= $($file.Name)"
		ollama create $tag -f $tmpModelfile
		$created += $tag
	}
	finally {
		Remove-Item -Path $tmpModelfile -Force -ErrorAction SilentlyContinue
	}
}

Write-Host ""
Write-Host "登録完了。利用可能モデル:"
$created | ForEach-Object { Write-Host " - $_" }

Write-Host ""
Write-Host "実行例:"
$created | ForEach-Object { Write-Host "ollama run $_" }
