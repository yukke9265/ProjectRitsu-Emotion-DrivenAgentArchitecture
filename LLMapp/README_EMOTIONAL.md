# 感情駆動型AIエージェント・アーキテクチャ

5つのモジュールで構成された感情を持つAIエージェントの実装です。

## モジュール構成

### ① InputAnalyzer（入力解析モジュール）
**ファイル**: `src/InputAnalyzer.h`, `src/InputAnalyzer.cpp`

ユーザーの発言を「感情生成」のために構造化します。

**機能**:
- トピック抽出
- 意図分類（質問/賞賛/批判/雑談など）
- AIへの評価判定（positive/neutral/negative）
- 感情スコア計算（-1.0 ~ 1.0）
- キーワード抽出

**主要メソッド**:
- `analyze()`: ユーザー発言を構造化データに変換
- `classify_intent()`: 発言の意図を分類
- `evaluate_ai_attitude()`: AIへの評価を判定

---

### ② EmotionEngine（感情エンジン）
**ファイル**: `src/EmotionEngine.h`, `src/EmotionEngine.cpp`

感情モジュールの実体であり、状態を管理する心臓部です。

**機能**:
- **Appraisal（評価）**: 入力内容と人格憲法を照らし合わせ、感情の変化量を計算
- **State Update**: 現在の感情値に変化量を加算
- **Decay（減衰）**: 時間経過による平時への復帰

**基本感情**:
- 喜び（JOY）
- 信頼（TRUST）
- 恐れ（FEAR）
- 驚き（SURPRISE）
- 悲しみ（SADNESS）
- 嫌悪（DISGUST）
- 怒り（ANGER）
- 期待（ANTICIPATION）

**主要メソッド**:
- `appraise_and_update()`: 入力に基づいて感情を評価・更新
- `apply_decay()`: 時間経過による感情の減衰
- `describe_emotion()`: 感情状態をテキスト表現に変換

---

### ③ MemoryController（記憶コントローラー）
**ファイル**: `src/MemoryController.h`, `src/MemoryController.cpp`

「物語的自己形成」を司るデータベース管理層です。

**機能**:
- **短期メモリ**: 直近N ターンの会話の流れを保持（デフォルト10ターン）
- **長期メモリ**: 過去の重要なエピソードを保存・検索
- **記憶の統合**: 短期メモリから重要なエピソードを抽出

**主要メソッド**:
- `add_to_short_term()`: 会話ターンを短期メモリに追加
- `search_episodes()`: キーワードに基づいて関連エピソードを検索
- `consolidate_memory()`: 短期メモリから重要なエピソードを抽出

---

### ④ PromptOrchestrator（プロンプト・オーケストレーター）
**ファイル**: `src/PromptOrchestrator.h`, `src/PromptOrchestrator.cpp`

各モジュールの情報を、最終的な「システムプロンプト」へ合成する組み立て役です。

**機能**:
- 人格憲法（固定）の提供
- 最新の感情数値（動的）の組み込み
- 関連する過去の記憶の統合
- トーン制御の指示生成

**生成するプロンプト構成**:
1. 人格憲法（システムプロンプト）
2. 現在の感情状態
3. トーン制御の指示
4. 短期メモリ（直近の会話）
5. 関連する長期記憶
6. ユーザー入力

**主要メソッド**:
- `build_final_prompt()`: 最終的なプロンプトを生成
- `set_system_prompt()`: システムプロンプトを設定

---

### ⑤ LLMInference（文章生成エンジン）
**ファイル**: `src/LLMInference.h`, `src/LLMInference.cpp`（既存）

実際にユーザーへの返答を生成する高機能LLMです。

**機能**:
- llama.cppを使用したローカルLLM推論
- 感情が注入されたプロンプトに従った応答生成

---

## 統合クラス: EmotionalAgent

**ファイル**: `src/EmotionalAgent.h`, `src/EmotionalAgent.cpp`

5つのモジュールを統合したエージェントクラスです。

**処理フロー**:
```
ユーザー入力
    ↓
① InputAnalyzer - 構造化
    ↓
② EmotionEngine - 感情更新
    ↓
③ MemoryController - 記憶追加
    ↓
④ PromptOrchestrator - プロンプト生成
    ↓
⑤ LLMInference - 応答生成
    ↓
出力
```

**主要メソッド**:
- `initialize()`: エージェントを初期化
- `process()`: ユーザー入力を処理して応答を生成
- `get_emotion_status()`: 現在の感情状態を取得
- `reset()`: エージェントをリセット

---

## 使用方法

### 1. 基本的な使用例（main_emotional.cpp）

```cpp
#include "EmotionalAgent.h"

// 人格憲法のカスタマイズ
PersonalityConstitution constitution;
constitution.core_values = "誠実で、親切で、ユーザーの成長を支援すること";
constitution.sensitivity_to_praise = 0.8;
constitution.sensitivity_to_criticism = 0.4;

// エージェントの作成
EmotionalAgent agent("path/to/model.gguf", constitution);

// 初期化
agent.initialize();

// 対話
std::string response = agent.process("こんにちは！");
std::cout << response << std::endl;

// 感情状態の確認
std::cout << agent.get_emotion_status() << std::endl;
```

### 2. コマンド

対話中に以下のコマンドが使用できます：

- `debug`: デバッグ情報を表示
- `emotion`: 現在の感情状態を表示
- `history`: 会話履歴を表示
- `reset`: エージェントをリセット
- `quit` / `exit`: 終了

---

## ビルド方法

Visual Studio 2022でプロジェクトをビルドしてください。

新しいエントリーポイント `main_emotional.cpp` が感情駆動型エージェントの実装です。

従来の単発推論は `main.cpp` に残っています。

---

## カスタマイズ

### 人格憲法のパラメータ

`PersonalityConstitution` 構造体で調整可能:

- `core_values`: コア価値観（文字列）
- `communication_style`: コミュニケーションスタイル（文字列）
- `sensitivity_to_praise`: 賞賛への感受性（0.0 ~ 1.0）
- `sensitivity_to_criticism`: 批判への感受性（0.0 ~ 1.0）
- `decay_rate`: 感情の減衰率（0.0 ~ 1.0）
- `baseline_valence`: 基準感情価（-1.0 ~ 1.0）

### 記憶の管理

- 短期メモリの最大ターン数: `MemoryController` コンストラクタで設定
- 長期メモリの最大エピソード数: `MemoryController.cpp` 内の定数で調整
- 記憶の統合頻度: `EmotionalAgent::consolidate_memories()` で制御

---

## ファイル一覧

```
LLMapp/
├── main.cpp                        # 従来の単発推論（既存）
├── main_emotional.cpp              # 感情駆動型エージェント（新規）
└── src/
    ├── InputAnalyzer.h             # 入力解析モジュール
    ├── InputAnalyzer.cpp
    ├── EmotionEngine.h             # 感情エンジン
    ├── EmotionEngine.cpp
    ├── MemoryController.h          # 記憶コントローラー
    ├── MemoryController.cpp
    ├── PromptOrchestrator.h        # プロンプト・オーケストレーター
    ├── PromptOrchestrator.cpp
    ├── EmotionalAgent.h            # 統合エージェント
    ├── EmotionalAgent.cpp
    ├── LLMInference.h              # LLM推論（既存）
    ├── LLMInference.cpp
    ├── DialogFunctions.h           # ダイアログ関数（既存）
    ├── DialogFunctions.cpp
    ├── PromptManager.h             # レガシー（スタブ）
    └── PromptManager.cpp
```

---

## 今後の拡張案

1. **より高度な自然言語処理**: 
   - トークナイズや形態素解析を活用した高精度なキーワード抽出

2. **ベクトルデータベースの統合**:
   - 長期記憶の検索をベクトル類似度で実装

3. **感情モデルの拡張**:
   - より複雑な感情モデル（PADモデル等）の導入

4. **永続化**:
   - 長期記憶をファイルやデータベースに保存

5. **マルチモーダル対応**:
   - 画像や音声入力からの感情推定

---

## ライセンス

このプロジェクトは既存のLLMappプロジェクトに準じます。
