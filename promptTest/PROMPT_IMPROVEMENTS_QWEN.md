# 元プロンプトからの改善まとめ（Qwen運用版）

## 1. 目的

元の検証用プロンプト（TestPrompt系）から、実運用で安定しやすい構成へ整理した変更点をまとめる。

## 2. 元プロンプトの主要課題

- 指示の重複
  - 出力契約や Response 指示が複数箇所に重複し、モデルが命令文を反復しやすかった。
- フェーズ境界の曖昧さ
  - Tool/Response の責務が混在し、Tool要求でも自然文へ逸脱するケースがあった。
- 履歴と命令の混線
  - 会話実例や長い補助説明が命令近傍にあり、プロンプト本文リークを誘発した。
- one-shot 検証性の弱さ
  - 実行時ログが不十分で、失敗時の原因追跡が難しかった。

## 3. 改善方針

- 1ファイル1目的（Response専用 / Tool専用）
- 契約は最小限かつ1回に集約
- 再出力指示は通常経路では使わず、必要時のみ
- 実行ログを自動保存し、raw出力まで追跡可能にする
- Qwen前提で運用版プロンプトを用意し、ランナーの既定値を運用版へ切替

## 4. 実装した改善（コード）

### 4.1 one-shot ログ強化

- one-shot 実行時に runtime ログを自動生成
- 記録項目: 実行開始、モデルパス、推論時間、応答本文、raw出力

対象:

- LLMapp/main_emotional.cpp
- LLMapp/src/EmotionalAgent.cpp

### 4.2 フェーズ専用モード追加

- Tool Phase 専用モードを追加
  - CLI: --once-system-prompt-tool-phase
  - 環境変数: LLMAPP_TOOL_PHASE_ONLY
- Response Phase 専用モードを追加
  - CLI: --once-system-prompt-response-phase
  - 環境変数: LLMAPP_RESPONSE_PHASE_ONLY
- 専用モード時は不要フェーズを抑制し、混線を低減

対象:

- LLMapp/main_emotional.cpp
- LLMapp/src/EmotionalAgent.cpp

### 4.3 安定性ランナー強化

- 反復実行（repeats）・タイムアウト・結果保存を自動化
- Toolテンプレート時は Tool Phase専用 one-shot を自動使用
- 厳格分類を追加
  - response_invalid_xxx / tool_invalid_xxx を分離
  - expected tool name 検証（例: get_current_time）
- 成功率（success / success_rate）を summary に出力

対象:

- scripts/oneshot_stability_runner.py

## 5. 実運用向けプロンプト（Qwen）

追加ファイル:

- promptTest/Prompt_operational_response_qwen.md
- promptTest/Prompt_operational_tool_qwen.md

意図:

- 元プロンプトの機能（人格方針・感情調律・ツール方針・ゲーム方針）を維持
- ただし重複命令や冗長説明を削減し、フェーズ責務を明確化

運用手順更新:

- promptTest/ONE_SHOT_USAGE.md

## 6. 実測で確認したポイント（抜粋）

### 6.1 フェーズ専用モード導入前

- Response 側は一定成功するが、指示エコー/リークが残る
- Tool 側は自然文逸脱や fallback が多い

### 6.2 Tool Phase専用モード導入後

- Tool の形式遵守は大幅改善
- 期待ツール名チェックを入れると、意味的な誤選択（echo / finish_tool_planning）も検出可能

### 6.3 モデル比較（単発）

- Qwen は response/tool の両立で最も安定
- 最終的に DEFAULT_MODEL_PATH も Qwen 設定

参考フォルダ:

- promptTest/oneshot_runs/20260304_165618
- promptTest/oneshot_runs/20260304_165005
- promptTest/oneshot_runs/20260304_163839

## 7. 現在の状態

- モデル: Qwen_Qwen3-4B-Instruct-2507-Q5_K_M（デフォルト）
- ランナー既定テンプレート: 運用版Qwenプロンプト
- one-shot で Tool/Response を分離検証可能
