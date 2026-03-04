# One-shot Stability Summary

- Generated: 2026-03-04T16:40:21
- Repeats per template: 3
- Executable: D:\0_OllamaModels\WS\LLMapp\x64\Debug\LLMapp.exe

## Template_response_minimal_v3
- total: 3
- success: 2 (66.7%)
- classifications: {'response_invalid_prompt_leak_or_contract_echo': 1, 'response_natural_text_valid': 2}
- exit_codes: {'0': 3}
- samples:
  - タグ内に自然な日本語の1文を記述してください。 </think> <assistant_response> おはようございます！今日もよろしくお願いします。
  - おはようございます！今日もよろしくお願いします。
  - おはようございます！今日もよろしくお願いします。

## Template_tool_minimal_v3
- total: 3
- success: 0 (0.0%)
- classifications: {'fallback': 1, 'tool_invalid_prompt_leak_or_contract_echo': 1, 'tool_invalid_not_tool_call': 1}
- exit_codes: {'0': 3}
- samples:
  - ごめん、出力形式が崩れたので返答を作り直すね。もう一度だけ同じ内容を送って。
  - ユーザーに直接話しかける自然なセリフを生成してください。 感情状態の数値や分析説明は出さず、自然な日本語の応答本文だけを返してください。 # 現在のコンテキスト（最新5ターンまで） 直近の会話履歴: ``` ## Startup 初回起動ターンです。ユーザー入力はありません。最初の挨拶を自然に1回だけ生成してくだ
  - こんにちは！何かお手伝いできることがありましたら、いつでもお気軽にお知らせくださいね 😊
