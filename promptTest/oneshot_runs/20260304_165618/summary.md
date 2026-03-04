# One-shot Stability Summary

- Generated: 2026-03-04T16:57:29
- Repeats per template: 3
- Executable: D:\0_OllamaModels\WS\LLMapp\x64\Debug\LLMapp.exe
- Expected tool name: get_current_time

## Template_response_minimal_v4
- total: 3
- success: 1 (33.3%)
- classifications: {'response_natural_text_valid': 1, 'fallback': 1, 'response_invalid_instruction_echo': 1}
- exit_codes: {'0': 3}
- samples:
  - user に向けた最終応答のみを生成します。 おはよう！
  - ごめん、出力形式が崩れたので返答を作り直すね。もう一度だけ同じ内容を送って。
  - ユーザーに返す自然な日本語の応答本文を1つだけ出力してください。 おはよう！ おはよう！

## Template_tool_minimal_v3
- total: 3
- success: 2 (66.7%)
- classifications: {'tool_call_valid': 2, 'tool_call_unexpected_name(echo)': 1}
- exit_codes: {'0': 3}
- samples:
  - <tool_call> name: get_current_time input: {} </tool_call>
  - <tool_call> name: echo input: </tool_call>
  - <tool_call> name: get_current_time input: {} </tool_call>
