# 感情駆動型AIエージェント（簡易メモ）

このファイルは「構成の見取り図」だけを置く軽量ドキュメントです。  
運用仕様の正本は、実装近接コメントを参照してください。

## 主要ファイル

- `src/EmotionalAgent.cpp`: 1ターン処理の主制御（Tool Phase / Response Phase）
- `src/ToolIO.h`, `src/ToolIO.cpp`: ツール呼び出し・応答のパース規約
- `src/ToolSetup.cpp`: 標準ツールの登録
- `src/PromptOrchestrator.cpp`: 最終プロンプト組み立て
- `src/Config.h`: モデル・プリセット設定

## 実行コマンド

- `debug`
- `emotion`
- `history`
- `reset`
- `prompt`
- `exit` / `quit`

## ビルドと実行

```powershell
.\scripts\build_minimal.ps1 -Configuration Debug -Platform x64
Set-Location .\LLMapp\x64\Debug
.\LLMapp.exe
```

## 補足

- 生成物（`agent_state.dat`, `x64/**`）は原則コミットしません。
- 仕様更新時はREADMEより先に実装コメントを更新してください。
