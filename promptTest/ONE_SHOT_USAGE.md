# ワンショット実行オプション（運用版）

## モード

- `--once-system-prompt` : 通常 one-shot（既存互換）
- `--once-system-prompt-tool-phase` : Tool Phase 専用 one-shot
- `--once-system-prompt-response-phase` : Response Phase 専用 one-shot

## コピペ用（Qwen運用）

### 1) 実行ファイルの場所へ移動

```powershell
Set-Location d:\0_OllamaModels\WS\LLMapp\x64\Debug
```

### 2) Tool Phase 専用 one-shot

```powershell
.\LLMapp.exe --once-system-prompt-tool-phase d:\0_OllamaModels\WS\promptTest\Prompt_operational_tool_qwen.md
```

### 3) Response Phase 専用 one-shot

```powershell
.\LLMapp.exe --once-system-prompt-response-phase d:\0_OllamaModels\WS\promptTest\Prompt_operational_response_qwen.md
```

## ログ

one-shot 実行時は `promptTest\_oneshot_runtime.log` に実行ログと raw 出力が保存されます。

## よくあるエラー

- オプションの後ろにファイルパスがない
- 指定したファイルが存在しない
- ファイルが空
