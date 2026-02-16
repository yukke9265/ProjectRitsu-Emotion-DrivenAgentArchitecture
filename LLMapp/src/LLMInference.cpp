#include "LLMInference.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <sstream>

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

        // サンプリングパラメータの設定（繰り返し防止）
        params_.sampling.penalty_repeat = 1.15f;  // repeat_penalty（強化）
        params_.sampling.penalty_last_n = 128;    // 直近128トークンを監視

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
    return cleanup_response(infer_raw(prompt));
}

std::string LLMInference::infer_raw(const std::string& prompt) {
    if (!initialized_) {
        last_error_ = "LLMがまだ初期化されていません。initialize()を呼び出してください。";
        return "";
    }

    try {
        // トークン化
        std::vector<llama_token> prompt_tokens = common_tokenize(ctx_, prompt, true);

        // 長文プロンプトは分割デコード（n_batch超過によるASSERT回避）
        constexpr int kDecodeChunkSize = 256;
        size_t offset = 0;
        while (offset < prompt_tokens.size()) {
            int chunk = static_cast<int>(std::min<size_t>(kDecodeChunkSize, prompt_tokens.size() - offset));
            if (llama_decode(ctx_, llama_batch_get_one(prompt_tokens.data() + offset, chunk))) {
                last_error_ = "プロンプトのデコードに失敗しました。";
                return "";
            }
            offset += static_cast<size_t>(chunk);
        }

        // 推論ループ
        std::string result;
        int n_cur = 0;
        std::vector<llama_token> tokens_list;

        while (n_cur < n_predict_ || n_predict_ == -1) {
            // デコード実行
            if (!tokens_list.empty()) {
                if (llama_decode(ctx_, llama_batch_get_one(tokens_list.data(), (int)tokens_list.size()))) {
                    last_error_ = "デコードに失敗しました。";
                    return result;
                }
                tokens_list.clear();
            }

            // トークンサンプリング
            auto id = common_sampler_sample(sampler_, ctx_, -1);
            common_sampler_accept(sampler_, id, true);

            // トークンを文字列に変換
            std::string token_str = common_token_to_piece(ctx_, id);
            result += token_str;

            // メタ出力パターンが出始めたら早期終了（過剰生成の抑止）
            if (result.find("\n→") != std::string::npos ||
                result.find("\n✅") != std::string::npos ||
                result.find("\n（※") != std::string::npos ||
                result.find("\nAI:") != std::string::npos ||
                result.find("\nYou:") != std::string::npos ||
                result.size() > 1200) {
                break;
            }

            // 終了判定（EOG: End of Generation）
            if (llama_vocab_is_eog(llama_model_get_vocab(model_), id)) {
                break;
            }

            tokens_list.push_back(id);
            n_cur++;
        }

        //printf("\n[debug]\n %s\n[debug_end]\n", result.c_str());

        return result;
    } catch (const std::exception& e) {
        last_error_ = std::string("推論中にエラーが発生しました: ") + e.what();
        return "";
    }
}

// 制御トークンと不要な文字列をクリーンアップするヘルパー関数
std::string LLMInference::cleanup_response(const std::string& raw_output) {
    std::string result = raw_output;

    // 改行コードを統一
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());

    // 1. 構造化フォーマットからセリフ部分を抽出
    size_t response_pos = result.find("応答:");
    if (response_pos != std::string::npos) {
        result = result.substr(response_pos + 6);  // "応答:" (6バイト) の後ろから
        
        // 直後の空白と ":" を削除
        size_t start = 0;
        while (start < result.length() && 
               (result[start] == ' ' || result[start] == '\t' || 
                result[start] == '\n' || result[start] == '\r' || 
                result[start] == ':')) {
            start++;
        }
        result = result.substr(start);
    }

    // 2. 感情状態の数値データを削除
    // "感情状態:" で始まる行を削除
    size_t emotion_pos = 0;
    while ((emotion_pos = result.find("感情状態:", emotion_pos)) != std::string::npos) {
        // 行の開始位置を探す
        size_t line_start = emotion_pos;
        while (line_start > 0 && result[line_start - 1] != '\n') {
            line_start--;
        }
        // 行の終了位置を探す
        size_t line_end = result.find('\n', emotion_pos);
        if (line_end == std::string::npos) {
            line_end = result.length();
        } else {
            line_end++; // 改行も含める
        }
        result.erase(line_start, line_end - line_start);
    }

    // "感情価:" "覚醒度:" も削除
    std::vector<std::string> patterns = {"感情価:", "覚醒度:", "Valence:", "Arousal:"};
    for (const auto& pattern : patterns) {
        size_t pos = 0;
        while ((pos = result.find(pattern, pos)) != std::string::npos) {
            size_t line_start = pos;
            while (line_start > 0 && result[line_start - 1] != '\n') {
                line_start--;
            }
            size_t line_end = result.find('\n', pos);
            if (line_end == std::string::npos) {
                line_end = result.length();
            } else {
                line_end++;
            }
            result.erase(line_start, line_end - line_start);
        }
    }

    // 3. メタ情報の削除（例、注釈など）
    // "（例：...）" パターンを削除
    size_t example_start = 0;
    while ((example_start = result.find("（例：", example_start)) != std::string::npos) {
        size_t example_end = result.find("）", example_start);
        if (example_end != std::string::npos) {
            result.erase(example_start, example_end - example_start + 3);  // "）" (3バイト)も含む
        } else {
            break;
        }
    }

    // "（※注：...）" パターンを削除
    size_t note_start = 0;
    while ((note_start = result.find("（※", note_start)) != std::string::npos) {
        size_t note_end = result.find("）", note_start);
        if (note_end != std::string::npos) {
            result.erase(note_start, note_end - note_start + 3);
        } else {
            break;
        }
    }

    // "---" 区切り線以降を削除
    size_t separator_pos = result.find("---");
    if (separator_pos != std::string::npos) {
        result = result.substr(0, separator_pos);
    }

    // 4. <|....|> 形式のすべての制御トークンを削除
    size_t pos = 0;
    while ((pos = result.find("<|", pos)) != std::string::npos) {
        size_t end_pos = result.find("|>", pos);
        if (end_pos != std::string::npos) {
            result.erase(pos, end_pos - pos + 2);
        } else {
            size_t close_pos = result.find(">", pos);
            if (close_pos != std::string::npos) {
                result.erase(pos, close_pos - pos + 1);
            } else {
                break;
            }
        }
    }

    // 5. 残りの < > パターンも削除
    pos = 0;
    while ((pos = result.find("<", pos)) != std::string::npos) {
        size_t end_pos = result.find(">", pos);
        if (end_pos != std::string::npos) {
            result.erase(pos, end_pos - pos + 1);
        } else {
            break;
        }
    }

    // 6. 先頭の改行と空白を削除
    size_t start = 0;
    while (start < result.length() && (result[start] == '\n' || result[start] == '\r' || result[start] == ' ' || result[start] == '\t')) {
        start++;
    }
    result = result.substr(start);

    // 7. 末尾の改行と空白を削除
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }

    // 8. 行ベースのフィルタ（メタ行を除去し、以降を打ち切る）
    auto ltrim = [](const std::string& s) {
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
            ++i;
        }
        return s.substr(i);
    };

    auto starts_with = [](const std::string& s, const std::string& prefix) {
        return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
    };

    auto is_meta_line = [&](const std::string& line) {
        std::string t = ltrim(line);
        return starts_with(t, "→") ||
               starts_with(t, "✅") ||
               starts_with(t, "（※") ||
               starts_with(t, "※") ||
               starts_with(t, "---") ||
               starts_with(t, "AI:") ||
               starts_with(t, "You:") ||
               starts_with(t, "感情状態:") ||
               starts_with(t, "感情価:") ||
               starts_with(t, "覚醒度:");
    };

    std::istringstream iss(result);
    std::ostringstream oss;
    std::string line;
    std::string prev_line;
    bool has_content = false;

    while (std::getline(iss, line)) {
        if (is_meta_line(line)) {
            if (has_content) {
                break;
            }
            continue;
        }

        if (line == prev_line && !line.empty()) {
            continue;
        }

        if (!line.empty()) {
            has_content = true;
        }

        if (oss.tellp() > 0) {
            oss << "\n";
        }
        oss << line;
        prev_line = line;
    }

    result = oss.str();

    // 9. 最終トリム
    while (!result.empty() && (result.back() == '\n' || result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }

    return result;
}

std::string LLMInference::infer_stateless(const std::string& prompt) {
    // 通常の推論を実行
    std::string result = infer(prompt);
    
    // KVキャッシュをクリアして次の推論に備える
    clear_kv_cache();
    
    return result;
}

void LLMInference::clear_kv_cache() {
    if (!initialized_ || !ctx_) {
        return;
    }
    
    // KVキャッシュをクリア（新しいメモリAPI使用）
    llama_memory_t mem = llama_get_memory(ctx_);
    llama_memory_clear(mem, true);  // data=true でデータバッファもクリア
    
    // サンプラーもリセット
    if (sampler_) {
        common_sampler_reset(sampler_);
    }
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
