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
