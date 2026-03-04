# 感情駆動型AIエージェント（Project Ritsu）

llama.cpp を使った C++17 ベースのローカル対話エージェントです。

## このREADMEの方針

- ここには「滅多に変わらない最小情報」だけを置きます。
- 変更頻度の高い仕様（出力契約・ツール運用・フェーズ制御）は、実装近接コメントを正本とします。

## クイックスタート

1. `models/` に GGUF モデルを配置
2. 必要なら `LLMapp/src/Config.h` の `DEFAULT_MODEL_PATH` を更新
3. ビルド

```powershell
.\scripts\build_minimal.ps1 -Configuration Debug -Platform x64
```

1. 実行

```powershell
Set-Location .\LLMapp\x64\Debug
.\LLMapp.exe
```

### システムプロンプトを1回だけテスト実行

```powershell
Set-Location .\LLMapp\x64\Debug
.\LLMapp.exe --once-system-prompt ..\..\promptTest\TestPrompt.md
```

- 任意の入力文を渡す場合:

```powershell
.\LLMapp.exe --once-system-prompt ..\..\promptTest\TestPrompt.md "このプロンプトで自己紹介して"
```

- 詳細なコピペ例: `promptTest/ONE_SHOT_USAGE.md`

## 実行時コマンド

- `debug`
- `emotion`
- `history`
- `reset`
- `prompt`
- `exit` / `quit`

## 参照先

- 実装の入口: `LLMapp/src/EmotionalAgent.cpp`
- ツールI/O仕様: `LLMapp/src/ToolIO.h`, `LLMapp/src/ToolIO.cpp`
- 標準ツール登録: `LLMapp/src/ToolSetup.cpp`
- 設定: `LLMapp/src/Config.h`

## ライセンス

MIT License。詳細は [LICENSE](LICENSE)。
