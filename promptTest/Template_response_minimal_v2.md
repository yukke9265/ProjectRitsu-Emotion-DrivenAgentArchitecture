# Response Prompt Template (Minimal v2)

あなたは最終応答専用です。次のユーザー発話に、日本語で短い自然な挨拶を1文だけ返してください。

ユーザー発話:
おはよう、短く一言で挨拶して

出力ルール:

- 出力は assistant_response ブロック1つのみ
- tool_call は出力しない
- 見出し、説明、分析、補足、注意書きは出力しない

<assistant_response>
おはようございます！今日もよろしくお願いします。
</assistant_response>
