# Copilot インストラクション - 感情駆動型AIエージェント

## プロジェクト概要

このワークスペースは、**感情を持つAIエージェント**を実装したC++プロジェクトです。llama.cppを活用し、ユーザーとの対話を通じて感情を変化させ、記憶を蓄積し、人格憲法に基づいた応答を生成します。

## アーキテクチャ

### 主要な5つのモジュール

1. **InputAnalyzer** (`src/InputAnalyzer.{h,cpp}`)
   - ユーザー発言の構造化解析
   - 意図分類（質問/賞賛/批判/雑談など）
   - AIへの評価判定（positive/neutral/negative）
   - 感情スコア計算（-1.0 ~ 1.0）
   - LLMベース + キーワードベースのハイブリッド解析

2. **EmotionEngine** (`src/EmotionEngine.{h,cpp}`)
   - 8つの基本感情管理（喜び、信頼、恐れ、驚き、悲しみ、嫌悪、怒り、期待）
   - Appraisal（評価）：入力内容と人格憲法に基づく感情変化の計算
   - State Update：感情値の更新
   - Decay（減衰）：時間経過による平時への復帰
   - 感情状態のテキスト記述生成

3. **MemoryController** (`src/MemoryController.{h,cpp}`)
   - 短期記憶：直近の会話履歴（順序保持）
   - 長期記憶：重要なエピソードの保存（感情的に重要な出来事）
   - 記憶検索：関連する過去の記憶の取得

4. **PromptOrchestrator** (`src/PromptOrchestrator.{h,cpp}`)
   - 人格憲法 + 現在の感情 + 関連記憶を統合
   - LLM向け最終プロンプトの生成
   - マークダウン形式での構造化

5. **LLMInference** (`src/LLMInference.{h,cpp}`)
   - llama.cppラッパー
   - ステートフル推論（KVキャッシュ保持）とステートレス推論
   - 生出力推論 `infer_raw()`（ツール呼び出しタグの検出向け）
   - 出力後処理 `cleanup_response()`
   - GGUF形式モデルのサポート

### ツールI/Oモジュール（追加実装）

6. **ToolIO** (`src/ToolIO.{h,cpp}`)
   - LLMのツール呼び出しプロトコル定義（`<tool_call>...</tool_call>`）
   - ツール実行結果プロトコル定義（`<tool_result>...</tool_result>`）
   - ツールレジストリ（`ToolRegistryExecutor`）
   - 厳密JSONパーサ（`ToolJsonInput`）
   - オブジェクト入力スキーマ検証（必須キー・型・追加キー制御）

7. **ToolSetup** (`src/ToolSetup.{h,cpp}`)
   - `main_emotional.cpp` から分離した標準ツール登録処理
   - `register_default_tools(EmotionalAgent&)` でツール群を一括登録

### 統合クラス

- **EmotionalAgent** (`src/EmotionalAgent.{h,cpp}`)
   - 上記主要モジュールを統合
  - メインの対話インターフェース
  - `process(user_input)` で入力を処理し応答を生成
   - ツール実行器の注入/内蔵登録APIを提供（`set_tool_executor`, `register_tool`）

## プロジェクト構造

```
WS/
├── LLMapp/                      # メインアプリケーション
│   ├── main.cpp                 # エントリーポイント（基本版）
│   ├── main_emotional.cpp       # エントリーポイント（感情エージェント版）
│   ├── LLMapp.sln              # Visual Studio ソリューション
│   ├── LLMapp.vcxproj          # Visual Studio プロジェクト
│   ├── README_EMOTIONAL.md     # アーキテクチャ詳細ドキュメント
│   └── src/                     # ソースコード
│       ├── Config.h             # グローバル設定（モデルパス、パラメータ）
│       ├── EmotionalAgent.*     # 統合エージェント
│       ├── InputAnalyzer.*      # 入力解析
│       ├── EmotionEngine.*      # 感情エンジン
│       ├── MemoryController.*   # 記憶管理
│       ├── PromptOrchestrator.* # プロンプト生成
│       ├── LLMInference.*       # LLM推論
│       ├── ToolIO.*             # ツールI/Oプロトコル・実行・JSONスキーマ検証
│       ├── ToolSetup.*          # 標準ツール登録（mainから分離）
│       └── DialogFunctions.*    # ダイアログユーティリティ
├── external/
│   └── llama.cpp/               # llama.cpp ライブラリ（サブモジュール）
├── models/                      # GGUF モデルファイル
├── scripts/
│   └── build_and_run.ps1        # ビルド&実行スクリプト
└── tests/                       # テストファイル
```

## 技術スタック

- **言語**: C++17
- **ビルドシステム**: Visual Studio 2022, CMake
- **LLMライブラリ**: llama.cpp
- **モデル形式**: GGUF
- **プラットフォーム**: Windows (PowerShell スクリプト対応)

## 開発ガイドライン

### コーディングスタイル
- C++標準ライブラリを優先使用
- スマートポインタ（`std::unique_ptr`, `std::shared_ptr`）の活用
- ヘッダーファイルに詳細なコメント（Doxygen形式推奨）
- モジュール間の疎結合を維持

### 重要な設定ファイル

#### Config.h
- `DEFAULT_MODEL_PATH`: 使用するGGUFモデルのパス
- `DEFAULT_GPU_LAYERS`: GPUレイヤー数（99 = 全てGPU）
- `DEFAULT_CONTEXT_SIZE`: コンテキストサイズ（デフォルト: 8192）
- `DEFAULT_N_PREDICT`: 生成トークン数（-1 = 無制限）

### ビルド方法

1. **Visual Studio でビルド**
   ```
   LLMapp.sln を開く → ビルド → 実行
   ```

2. **PowerShell スクリプトでビルド&実行**
   ```powershell
   .\scripts\build_and_run.ps1
   ```

### テストモード

- `main_emotional.cpp` にテストモードが実装済み
- InputAnalyzerのLLMモードとキーワードモードの比較テスト
- テストケースは多様な入力パターンをカバー

## 主要な実装パターン

### 1. 感情の更新フロー
```cpp
user_input → InputAnalyzer::analyze() 
          → EmotionEngine::appraise_and_update() 
          → 感情値更新
```

### 2. 応答生成フロー
```cpp
user_input → EmotionalAgent::process()
          → InputAnalyzer（解析）
          → EmotionEngine（感情更新）
          → MemoryController（記憶追加・検索）
          → PromptOrchestrator（プロンプト生成）
          → LLMInference::infer_raw()（一次出力）
          → ToolIOProtocol::try_parse_tool_call()（ツール呼び出し判定）
          → ToolRegistryExecutor::execute()（必要時のみ）
          → tool_resultを再注入して再推論
          → LLMInference::cleanup_response()（最終整形）
          → 応答
```

### 3. 記憶の管理
```cpp
// 短期記憶: 全ての会話を順序付きで保存
MemoryController::add_short_term_memory(user, ai)

// 長期記憶: 重要度の高いエピソードのみ保存
MemoryController::add_to_long_term_memory(episode)
```

## 依存関係

### 外部ライブラリ
- **llama.cpp**: LLM推論エンジン
  - `external/llama.cpp/` に配置
  - ビルドディレクトリ: `external/llama.cpp/build/`
  - 必要なヘッダー: `common.h`, `llama.h`, `sampling.h`
  - 必要なライブラリ: `common.lib`, `llama.lib`

### モデルファイル
- GGUF形式（量子化済みモデル）
- `models/` ディレクトリに配置
- 例: `Qwen_Qwen3-4B-Instruct-2507-Q5_K_M.gguf`

## よくある作業

### 新しい感情を追加する
1. `EmotionEngine.h` の `EmotionType` enum に追加
2. `EmotionEngine::describe_emotion()` に記述ロジック追加
3. `EmotionEngine::appraise_and_update()` に評価ロジック追加

### 新しいインテント（意図）を追加する
1. `InputAnalyzer.h` の `Intent` enum に追加
2. `InputAnalyzer::classify_intent()` にキーワードマッチング追加
3. （LLMモード）プロンプトに新しいカテゴリーを追加

### モデルを変更する
1. `Config.h` の `DEFAULT_MODEL_PATH` を更新
2. 必要に応じて `DEFAULT_CONTEXT_SIZE` を調整
3. リビルド & 実行

### プロンプトをカスタマイズする
1. `PersonalityConstitution` 構造体を編集（人格憲法）
2. `PromptOrchestrator::set_system_prompt()` でカスタムプロンプト設定
3. `PromptOrchestrator::build_final_prompt()` でフォーマット調整

### ツールを追加する（推奨手順）
1. `src/ToolSetup.cpp` の `register_default_tools()` に `agent.register_tool(...)` を追加
2. JSON入力を使う場合は `ToolObjectSchema` を併用して型・必須キーを宣言
3. 追加キーを禁止したい場合は `allow_additional_keys = false` を設定
4. `scripts/build_minimal.ps1` でビルド確認

## デバッグヒント

- **LLM初期化失敗**: モデルパス、GPUメモリ、llama.cppのビルドを確認
- **感情が変化しない**: `EmotionEngine::appraise_and_update()` のロジックをチェック
- **応答が遅い**: `DEFAULT_CONTEXT_SIZE` を削減、GPUレイヤーを確認
- **記憶が保存されない**: `MemoryController` の閾値設定を確認

## 関連ドキュメント

- [LLMapp/README_EMOTIONAL.md](LLMapp/README_EMOTIONAL.md): 詳細なアーキテクチャ説明
- [external/llama.cpp/README.md](external/llama.cpp/README.md): llama.cpp ドキュメント

## 注意事項

- このプロジェクトはローカルLLMを使用（オフライン動作可能）
- GGUFモデルは大容量（数GB）のため、Git LFSまたは手動配置を推奨
- Visual Studio 2022 以降を推奨（C++17サポート必須）
- Windowsでのビルドとテストがメインターゲット
