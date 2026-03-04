# Tool Prompt Template (Minimal)

このプロンプトはツール計画・呼び出し（Tool Phase）専用です。
自然文の返答は出力しません。

ユーザー発話:
get_current_timeを実行して、現在時刻を確認したい

利用可能ツール:

- get_current_time
- finish_tool_planning

今回の期待動作:

- get_current_time を呼び出す
- 出力は tool_call ブロック1つだけ
- 前置き・説明文・箇条書き・コードブロックは禁止
- assistant_response は出力しない

<tool_call>
name: <tool_name>
input:
<tool_input_text_or_json>
</tool_call>
