#pragma once

// ===== LLMモデル設定 =====
// ここを変更するだけで、全てのプログラムで使用するモデルが変わります
#define DEFAULT_MODEL_PATH "D:\\0_OllamaModels\\WS\\models\\Qwen_Qwen3-4B-Instruct-2507-Q5_K_M.gguf"

// ===== LLMパラメータ設定 =====
#define DEFAULT_GPU_LAYERS 99        // GPU レイヤー数（99 = 全て）
#define DEFAULT_CONTEXT_SIZE 8192    // コンテキストサイズ
#define DEFAULT_N_PREDICT 256        // 生成トークン数（256 = 簡潔な応答、-1 = 無制限は非推奨）

// ===== テストモード用設定 =====
#define TEST_CONTEXT_SIZE 2048       // テスト時のコンテキストサイズ（小さめ）
#define TEST_N_PREDICT 128           // テスト時の生成トークン数（制限あり）

// ===== InputAnalyzer LLM設定 =====
#define LLM_PARSE_RETRY_COUNT 3      // JSONパース失敗時のリトライ回数
