# Copilot Instructions - 感情駆動型AIエージェント

## 1. このリポジトリの目的

このワークスペースは、**感情を持つローカルLLMエージェント**を C++17 で実装したプロジェクトです。  
llama.cpp を使い、人格憲法・感情状態・記憶・ツール実行を統合して応答を生成します。

## 2. 主要モジュール

1. **InputAnalyzer** (`LLMapp/src/InputAnalyzer.{h,cpp}`)
   - 意図分類（質問/賞賛/批判/雑談など）
   - AIへの評価（positive/neutral/negative）
   - 感情スコア計算（-1.0 ~ 1.0）
   - LLM + キーワードのハイブリッド解析

2. **EmotionEngine** (`LLMapp/src/EmotionEngine.{h,cpp}`)
   - 8基本感情（JOY/TRUST/FEAR/SURPRISE/SADNESS/DISGUST/ANGER/ANTICIPATION）
   - 評価→更新→減衰（Appraisal / Update / Decay）
   - 感情状態記述の生成

3. **MemoryController** (`LLMapp/src/MemoryController.{h,cpp}`)
   - 短期記憶（会話履歴）
   - 長期記憶（重要エピソード）
   - 関連記憶検索

4. **PromptOrchestrator** (`LLMapp/src/PromptOrchestrator.{h,cpp}`)
   - 人格憲法 + 感情状態 + 記憶 + システムログの統合
   - 最終プロンプト生成
   - `Output Contract` の独立セクション化
   - `response_style_instruction` の一元管理

5. **LLMInference** (`LLMapp/src/LLMInference.{h,cpp}`)
   - llama.cppラッパー
   - `infer_raw()` / `infer()` / `cleanup_response()`

6. **ToolIO** (`LLMapp/src/ToolIO.{h,cpp}`)
   - ツール呼び出し仕様 (`tool_channel` / `tool_call`)
   - 通常応答仕様 (`assistant_channel` / `assistant_response`)
   - 出力契約ガイド生成
   - 厳密JSONパース + スキーマ検証

7. **ToolSetup** (`LLMapp/src/ToolSetup.{h,cpp}`)
   - 標準ツール登録
   - `echo`, `get_current_time`, `sum_numbers`, `number_guess_game`

8. **EmotionalAgent** (`LLMapp/src/EmotionalAgent.{h,cpp}`)
   - 全モジュール統合
   - `process()`（本処理）
   - `build_prompt_preview()`（状態更新なしのプロンプト確認）

## 3. 実行時コマンド（対話中）

`main_emotional.cpp` では以下のコマンドを受け付けます。

- `debug`
- `emotion`
- `history`
- `reset`
- `prompt`（最終プロンプトのプレビュー）
- `exit` / `quit`

## 4. 出力契約とツール運用（重要）

### 通常応答

- `assistant_channel` のみを出力
- 本文は `assistant_response` のみ

### ツール呼び出し

- 必要なときだけ `tool_channel` 内で `tool_call` を出力
- 説明文や前置きを混在させない

### 数当てゲーム

- ゲーム判定は `number_guess_game` の結果に必ず従う
- 決め打ちで正誤を文章生成しない
- 使い分け:
  - 開始 `action=start`
  - 推測 `action=guess`
  - 状態 `action=status`
  - リセット `action=reset`

## 5. 開発ルール

- 既存設計を尊重し、最小差分で修正
- 仕様変更時は `README.md` / `LLMapp/README_EMOTIONAL.md` / 本ファイルの整合を取る
- 生成物や状態ファイルは原則コミットしない
  - 例: `agent_state.dat`, `LLMapp/x64/**`
- モデル切替は `LLMapp/src/Config.h` の `DEFAULT_MODEL_PATH` を変更

## 6. ビルドと確認

### ビルド

```powershell
.\scripts\build_minimal.ps1 -Configuration Debug -Platform x64
```

### 実行

```powershell
Set-Location .\LLMapp\x64\Debug
.\LLMapp.exe
```

### プロンプト確認

対話中に `prompt` を入力すると、状態更新なしで最終プロンプトを表示します。

## 7. 変更時のチェックポイント

- Prompt構成を変えたら:
  - `prompt` 出力で章順・重複・`Output Contract` 配置を確認
- ToolIOを変えたら:
  - `assistant_channel` と `tool_channel` の排他性を確認
- ツール追加時:
  - `ToolSetup.cpp` と `ToolIO` のガイド文の整合を確認

## 8. 参照

- `README.md`（ルート）
- `LLMapp/README_EMOTIONAL.md`
- `LLMapp/src/Config.h`

## 9. ドキュメント作成規約（重要）

- READMEは最小限に保つ（目的・起動手順・参照先のみ）。
- 変更頻度の高い仕様（出力契約、フェーズ制御、ツール運用）は、実装近接コメントを正本とする。
- 仕様変更時は、まず実装近接コメントを更新し、その後READMEは要約レベルのみ整合を取る。
- 実装近接コメントは、仕様が実際に評価される関数/処理ブロックの直上に記載する。
   - 例: `EmotionalAgent::process()` の Tool Phase / Response Phase 周辺
   - 例: `ToolIOProtocol` の契約生成・パース関数周辺
- 長文説明をREADMEへ再展開しない。詳細はコードとコメントを一次情報源として扱う。
