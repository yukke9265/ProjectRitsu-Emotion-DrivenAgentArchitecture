# One-shot Stability Summary

- Generated: 2026-03-04T17:06:21
- Repeats per template: 1
- Executable: D:\0_OllamaModels\WS\LLMapp\x64\Debug\LLMapp.exe
- Model path override: D:\0_OllamaModels\WS\models\NVIDIA-Nemotron-Nano-9B-v2-Japanese-Q4_K_M.gguf
- Expected tool name: get_current_time

## Template_response_minimal_v4
- total: 1
- success: 0 (0.0%)
- classifications: {'fallback': 1}
- exit_codes: {'0': 1}
- samples:
  - ごめん、出力形式が崩れたので返答を作り直すね。もう一度だけ同じ内容を送って。

## Template_tool_minimal_v3
- total: 1
- success: 0 (0.0%)
- classifications: {'tool_call_unexpected_name(finish_tool_planning)': 1}
- exit_codes: {'0': 1}
- samples:
  - <tool_call> name: finish_tool_planning input: {} </tool_call>
