#include "LLMInference.h"
#include <iostream>
#include <algorithm>
#include <cctype>

// 前方宣言
static std::string cleanup_output(const std::string& raw_output);

LLMInference::LLMInference(
    const std::string& model_path,
    int n_gpu_layers,
    int n_ctx,
    int n_predict)
    : n_predict_(n_predict), initialized_(false), model_(nullptr), 
      ctx_(nullptr), sampler_(nullptr) {
    
    params_.model.path = model_path;
    params_.n_gpu_layers = n_gpu_layers;
    params_.n_ctx = n_ctx;
}

LLMInference::~LLMInference() {
    cleanup();
}

bool LLMInference::initialize() {
    try {
        // バックエンド初期化
        llama_backend_init();

        // モデルと コンテキスト初期化
        llama_init_ = std::shared_ptr<common_init_result>(
            common_init_from_params(params_).release(),
            [](common_init_result* p) { 
                if (p) {
                    delete p;
                }
            }
        );

        if (!llama_init_ || !llama_init_->model()) {
            last_error_ = "モデルのロードに失敗しました。パスを確認してください。";
            return false;
        }

        // ポインタ取得
        model_ = llama_init_->model();
        ctx_ = llama_init_->context();

        if (!model_ || !ctx_) {
            last_error_ = "モデルまたはコンテキストの取得に失敗しました。";
            cleanup();
            return false;
        }

        // サンプラー初期化
        sampler_ = common_sampler_init(model_, params_.sampling);
        if (!sampler_) {
            last_error_ = "サンプラーの初期化に失敗しました。";
            cleanup();
            return false;
        }

        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("初期化中にエラーが発生しました: ") + e.what();
        cleanup();
        return false;
    }
}

std::string LLMInference::infer(const std::string& prompt) {
    if (!initialized_) {
        last_error_ = "LLMがまだ初期化されていません。initialize()を呼び出してください。";
        return "";
    }

    try {
        // トークン化
        std::vector<llama_token> tokens_list = common_tokenize(ctx_, prompt, true);

        // 推論ループ
        std::string result;
        int n_cur = 0;

        while (n_cur < n_predict_ || n_predict_ == -1) {
            // デコード実行
            if (llama_decode(ctx_, llama_batch_get_one(tokens_list.data(), (int)tokens_list.size()))) {
                last_error_ = "デコードに失敗しました。";
                return result;
            }
            tokens_list.clear();

            // トークンサンプリング
            auto id = common_sampler_sample(sampler_, ctx_, -1);
            common_sampler_accept(sampler_, id, true);

            // トークンを文字列に変換
            std::string token_str = common_token_to_piece(ctx_, id);
            result += token_str;

            // 終了判定（EOG: End of Generation）
            if (llama_vocab_is_eog(llama_model_get_vocab(model_), id)) {
                break;
            }

            tokens_list.push_back(id);
            n_cur++;
        }

        //printf("\n[debug]\n %s\n[debug_end]\n", result.c_str());

        return cleanup_output(result);
    } catch (const std::exception& e) {
        last_error_ = std::string("推論中にエラーが発生しました: ") + e.what();
        return "";
    }
}

// 制御トークンと不要な文字列をクリーンアップするヘルパー関数
static std::string cleanup_output(const std::string& raw_output) {
    std::string result = raw_output;

    // <|....|> 形式のすべての制御トークンを削除
    size_t pos = 0;
    while ((pos = result.find("<|", pos)) != std::string::npos) {
        size_t end_pos = result.find("|>", pos);
        if (end_pos != std::string::npos) {
            result.erase(pos, end_pos - pos + 2);
        } else {
            // |> が見つからない場合は、< から > までを削除
            size_t close_pos = result.find(">", pos);
            if (close_pos != std::string::npos) {
                result.erase(pos, close_pos - pos + 1);
            } else {
                break;
            }
        }
    }

    // 残りの < > パターンも削除
    pos = 0;
    while ((pos = result.find("<", pos)) != std::string::npos) {
        size_t end_pos = result.find(">", pos);
        if (end_pos != std::string::npos) {
            result.erase(pos, end_pos - pos + 1);
        } else {
            break;
        }
    }

    // 先頭の改行と空白を削除
    size_t start = 0;
    while (start < result.length() && (result[start] == '\n' || result[start] == '\r' || result[start] == ' ' || result[start] == '\t')) {
        start++;
    }
    result = result.substr(start);

    // 末尾の改行と空白を削除
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }

    return result;
}

void LLMInference::cleanup() {
    if (sampler_) {
        common_sampler_free(sampler_);
        sampler_ = nullptr;
    }

    if (llama_init_) {
        llama_init_.reset();
    }

    model_ = nullptr;
    ctx_ = nullptr;

    llama_backend_free();
    initialized_ = false;
}
