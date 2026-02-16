#pragma once

#include "common.h"
#include "llama.h"
#include "sampling.h"
#include <string>
#include <memory>
#include <vector>

class LLMInference {
public:
    // コンストラクタ：初期設定
    LLMInference(
        const std::string& model_path,
        int n_gpu_layers = 99,
        int n_ctx = 2048,
        int n_predict = -1
    );

    // デストラクタ：後片付け
    ~LLMInference();

    // 初期化処理
    bool initialize();

    // 推論実行：プロンプトを受け取って結果を返す
    std::string infer(const std::string& prompt);

    // 推論実行（生出力）：後処理を行わず、モデルの出力をそのまま返す
    // ツール呼び出しタグ（<tool_call>...</tool_call>）検出時に使用する
    std::string infer_raw(const std::string& prompt);

    // 出力後処理（メタ情報や制御トークン除去）
    // infer_raw() の戻り値を既存仕様に合わせる際に利用できる
    static std::string cleanup_response(const std::string& raw_output);

    // 単発推論：推論後に自動的にKVキャッシュをクリア（テストや独立した推論に最適）
    std::string infer_stateless(const std::string& prompt);

    // KVキャッシュをクリア（メモリを解放して次の推論に備える）
    void clear_kv_cache();

    // 内部状態の確認
    bool is_initialized() const { return initialized_; }

    // エラーメッセージの取得
    std::string get_last_error() const { return last_error_; }

private:
    // パラメータ
    common_params params_;
    int n_predict_;
    bool initialized_;
    std::string last_error_;

    // LLamaポインタ
    std::shared_ptr<common_init_result> llama_init_;
    llama_model* model_;
    llama_context* ctx_;
    common_sampler* sampler_;

    // ヘルパーメソッド
    void cleanup();
};
