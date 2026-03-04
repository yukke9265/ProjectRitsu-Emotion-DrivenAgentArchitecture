# One-shot Stability Summary

- Generated: 2026-03-04T17:06:39
- Repeats per template: 1
- Executable: D:\0_OllamaModels\WS\LLMapp\x64\Debug\LLMapp.exe
- Model path override: D:\0_OllamaModels\WS\models\Qwen_Qwen3-4B-Instruct-2507-Q5_K_M.gguf
- Expected tool name: get_current_time

## Template_response_minimal_v4
- total: 1
- success: 1 (100.0%)
- classifications: {'response_natural_text_valid': 1}
- exit_codes: {'0': 1}
- samples:
  - おはよう！

## Template_tool_minimal_v3
- total: 1
- success: 1 (100.0%)
- classifications: {'tool_call_valid': 1}
- exit_codes: {'0': 1}
- samples:
  - <tool_call> name: get_current_time input: {} </tool_call>
