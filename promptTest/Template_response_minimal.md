# Response Prompt Template (Minimal)

このプロンプトは最終応答（Response Phase）専用です。

ユーザー発話:
おはよう、短く一言で挨拶して

出力要件:

- 日本語で短く自然に返答する
- 前置き・説明・分析・見出しを出さない
- tool_call を出さない
- assistant_response ブロック1つだけを出力する

<assistant_response>
<ユーザーに返す自然な日本語の応答本文>
</assistant_response>
