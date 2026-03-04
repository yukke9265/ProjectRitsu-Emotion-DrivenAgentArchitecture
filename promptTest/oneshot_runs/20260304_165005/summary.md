# One-shot Stability Summary

- Generated: 2026-03-04T16:51:13
- Repeats per template: 3
- Executable: D:\0_OllamaModels\WS\LLMapp\x64\Debug\LLMapp.exe

## Template_response_minimal_v4
- total: 3
- success: 1 (33.3%)
- classifications: {'response_natural_text_valid': 1, 'response_invalid_prompt_leak_or_contract_echo': 2}
- exit_codes: {'0': 3}
- samples:
  - おはよう！
  - ユーザーに直接話しかける自然なセリフを生成してください。 感情状態の数値や分析説明は出さず、自然な日本語の応答本文だけを返してください。 # ユーザー発話 おはよう、短く一言で挨拶して
  - ユーザーに返す応答本文を1つだけ出力してください。 # 現在のあなたの感情状態と応答トーン あなたは今、落ち着いた中立的な気分です。バランスの取れたトーンで回答してください。 少し活気を持たせた表現を心がけてください。

## Template_tool_minimal_v3
- total: 3
- success: 3 (100.0%)
- classifications: {'tool_call_valid': 3}
- exit_codes: {'0': 3}
- samples:
  - <tool_call> name: number_guess_game input: { "action": "start", "min": 1, "max": 50 } </tool_call>
  - <tool_call> name: get_current_time input: {} </tool_call>
  - <tool_call> name: get_current_time input: {} </tool_call>
