import requests

# モデル名をここで設定
MODEL = "LLM1"

# Ollamaのローカルサーバーにリクエストを送信
response = requests.post(
    "http://localhost:11434/api/generate",
    json={
        "model": MODEL,
        "prompt": "日本の首都はどこですか？",
        "stream": False
    }
)

# レスポンスを表示
print(response.json())

##D:\0_OllamaModels\WS\llama\llama.cpp\build\bin\Release\llama-cli.exe -m models\LFM2.5-1.2B-Instruct-BF16.gguf -f test.txt