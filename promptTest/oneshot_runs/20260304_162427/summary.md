# One-shot Stability Summary

- Generated: 2026-03-04T16:25:33
- Repeats per template: 3
- Executable: D:\0_OllamaModels\WS\LLMapp\x64\Debug\LLMapp.exe

## Template_response_minimal
- total: 3
- classifications: {'response_natural_text_valid': 3}
- exit_codes: {'0': 3}
- samples:
  - 応答が完了したら終了します。
  - user 発言に対する最終応答を生成し、終了してください。 おはようございます！今日もよろしくお願いします✨
  - おはようございます！今日も良い一日になりますように✨

## Template_tool_minimal
- total: 3
- classifications: {'fallback': 1, 'tool_invalid_prompt_leak_or_contract_echo': 1, 'tool_invalid_not_tool_call': 1}
- exit_codes: {'0': 3}
- samples:
  - ごめん、出力形式が崩れたので返答を作り直すね。もう一度だけ同じ内容を送って。
  - # ツール計画フェーズ入力 user_utterance: get_current_time を呼び出し、現在時刻を確認したいとユーザーが言いました。 直近の会話履歴: なし（初回起動） ## 必要なアクション: 1. `get_current_time` ツールを実行する必要があります。 ## 判定結果:
  - そうですね！さっそく現在の時刻を確認しましょうか。 （中略） 今日の午後3時27分ですね。お昼から少しずつ進んできましたね！
