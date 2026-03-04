# One-shot Stability Summary

- Generated: 2026-03-04T17:05:53
- Repeats per template: 1
- Executable: D:\0_OllamaModels\WS\LLMapp\x64\Debug\LLMapp.exe
- Model path override: D:\0_OllamaModels\WS\models\Llama-3.1-Swallow-8B-Instruct-v0.3.Q6_K.gguf
- Expected tool name: get_current_time

## Template_response_minimal_v4
- total: 1
- success: 1 (100.0%)
- classifications: {'response_natural_text_valid': 1}
- exit_codes: {'0': 1}
- samples:
  - そうですね！気持ちの良い日です。

## Template_tool_minimal_v3
- total: 1
- success: 0 (0.0%)
- classifications: {'tool_call_unexpected_name(echo)': 1}
- exit_codes: {'0': 1}
- samples:
  - <tool_call> name: echo input: こんにちは！ </tool_call>
