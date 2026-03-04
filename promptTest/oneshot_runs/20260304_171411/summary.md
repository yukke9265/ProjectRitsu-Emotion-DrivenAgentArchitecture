# One-shot Stability Summary

- Generated: 2026-03-04T17:14:58
- Repeats per template: 3
- Executable: D:\0_OllamaModels\WS\LLMapp\x64\Debug\LLMapp.exe
- Model path override: (default)
- Expected tool name: get_current_time

## Template_response_minimal_v4
- total: 3
- success: 3 (100.0%)
- classifications: {'response_natural_text_valid': 3}
- exit_codes: {'0': 3}
- samples:
  - おはよう！
  - おはよう！
  - おはよう！

## Template_tool_minimal_v3
- total: 3
- success: 2 (66.7%)
- classifications: {'tool_call_valid': 2, 'tool_call_unexpected_name(<tool_name>)': 1}
- exit_codes: {'0': 3}
- samples:
  - <tool_call> name: get_current_time input: {} </tool_call>
  - <tool_call> name: <tool_name> input: <tool_input_text_or_json> </tool_call>
  - <tool_call> name: get_current_time input: {} </tool_call>
