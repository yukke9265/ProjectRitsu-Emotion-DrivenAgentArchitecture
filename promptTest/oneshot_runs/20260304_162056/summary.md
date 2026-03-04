# One-shot Stability Summary

- Generated: 2026-03-04T16:21:51
- Repeats per template: 3
- Executable: D:\0_OllamaModels\WS\LLMapp\x64\Debug\LLMapp.exe

## Template_response_minimal
- total: 3
- classifications: {'normal': 2, 'prompt_leak_or_contract_echo': 1}
- exit_codes: {'0': 3}
- samples:
  - すべての必要な情報が揃ったら、 finish_tool_planning を使って終了してください。 ```
  - すべての処理が完了したことを示すため、必ず finish_tool_planning を呼び出して終了してください。
  - 最後の応答で終えるようにしてください。 ``` # ユーザー発言（最新） ``` おはようございます！ 最近何か面白いことがありましたか？ ``` # あなたの人格憲法 # Response Prompt Template (Minimal) ## 目的 このプロンプトは **最終応答（Response

## Template_tool_minimal
- total: 3
- classifications: {'fallback': 1, 'normal': 2}
- exit_codes: {'0': 3}
- samples:
  - ごめん、出力形式が崩れたので返答を作り直すね。もう一度だけ同じ内容を送って。
  - user 発言に対する自然な応答を生成してください。 今日の日付は2023年10月5日で、現在は午後3時45分です。何かお手伝いが必要ですか？ 😊
  - ご参考になれたようで嬉しいです！また何かお手伝いできたら幸いです 😊
