#include "LLMInference.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>
#include "ggml-backend.h"

namespace {
constexpr float kDefaultTemp = 1.0f;
constexpr float kDefaultTopP = 0.95f;
constexpr int32_t kDefaultTopK = 64;

constexpr float kStrictTemp = 0.0f;
constexpr float kStrictTopP = 1.0f;
constexpr int32_t kStrictTopK = 1;

bool contains_any(const std::string& text, const std::vector<std::string>& needles) {
    for (const auto& needle : needles) {
        if (text.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool is_contract_sensitive_prompt(const std::string& prompt) {
    static const std::vector<std::string> markers = {
        "Tool Phase 契約",
        "Response Phase 契約",
        "<tool_call>",
        "<assistant_response>"
    };
    return contains_any(prompt, markers);
}

// chat template は通常通り適用しつつ greedy サンプリングだけを強制する
// コンパクトリトライプロンプトなどに埋め込むマーカーで検出する
bool is_strict_sampling_only_prompt(const std::string& prompt) {
    return prompt.find("<!-- strict_sampling=true -->") != std::string::npos;
}

std::string format_prompt_with_model_chat_template(const llama_model* model, const std::string& prompt) {
    if (!model || prompt.empty()) {
        return prompt;
    }

    const char* tmpl = llama_model_chat_template(model, nullptr);
    if (!tmpl || tmpl[0] == '\0') {
        return prompt;
    }

    const llama_chat_message chat[] = {
        { "user", prompt.c_str() }
    };

    int32_t required = llama_chat_apply_template(tmpl, chat, 1, true, nullptr, 0);
    if (required <= 0) {
        return prompt;
    }

    std::vector<char> buf(static_cast<size_t>(required) + 1, '\0');
    int32_t written = llama_chat_apply_template(
        tmpl,
        chat,
        1,
        true,
        buf.data(),
        static_cast<int32_t>(buf.size())
    );

    if (written <= 0) {
        return prompt;
    }

    return std::string(buf.data(), static_cast<size_t>(written));
}

void print_vram_free_debug() {
    bool gpu_device_found = false;

    for (size_t i = 0; i < ggml_backend_dev_count(); ++i) {
        ggml_backend_dev_t dev = ggml_backend_dev_get(i);
        if (!dev) {
            continue;
        }

        const enum ggml_backend_dev_type dev_type = ggml_backend_dev_type(dev);
        if (dev_type == GGML_BACKEND_DEVICE_TYPE_CPU) {
            continue;
        }

        gpu_device_found = true;
        size_t free_bytes = 0;
        size_t total_bytes = 0;
        ggml_backend_dev_memory(dev, &free_bytes, &total_bytes);

        std::cout << "[DEBUG][VRAM] "
                  << ggml_backend_dev_name(dev)
                  << ": free=" << (free_bytes / 1024 / 1024) << " MiB"
                  << " / total=" << (total_bytes / 1024 / 1024) << " MiB"
                  << std::endl;
    }

    if (!gpu_device_found) {
        std::cout << "[DEBUG][VRAM] GPUデバイス未検出（CPU実行）" << std::endl;
    }
}
}

LLMInference::LLMInference(
    const std::string& model_path,
    int n_gpu_layers,
    int n_ctx,
    int n_predict)
    : n_predict_(n_predict), initialized_(false), debug_mode_(false), model_(nullptr), 
      ctx_(nullptr), sampler_(nullptr) {
    
    params_.model.path = model_path;
    params_.n_gpu_layers = n_gpu_layers;
    params_.n_ctx = n_ctx;
    params_.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;
}

LLMInference::~LLMInference() {
    cleanup();
}

bool LLMInference::initialize() {
    try {
        // バックエンド初期化
        llama_backend_init();

        // サンプリングパラメータの設定（繰り返し防止）
        params_.sampling.temp = kDefaultTemp;
        params_.sampling.top_p = kDefaultTopP;
        params_.sampling.top_k = kDefaultTopK;
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
        const bool strict_contract_prompt = is_contract_sensitive_prompt(prompt);
        const bool use_strict_sampling = strict_contract_prompt || is_strict_sampling_only_prompt(prompt);

        common_params_sampling sampling_for_turn = params_.sampling;
        if (use_strict_sampling) {
            sampling_for_turn.temp = kStrictTemp;
            sampling_for_turn.top_p = kStrictTopP;
            sampling_for_turn.top_k = kStrictTopK;
        }

        if (sampler_) {
            common_sampler_free(sampler_);
            sampler_ = nullptr;
        }

        sampler_ = common_sampler_init(model_, sampling_for_turn);
        if (!sampler_) {
            last_error_ = "サンプラーの再初期化に失敗しました。";
            return "";
        }

        const std::string model_input = strict_contract_prompt
            ? prompt
            : format_prompt_with_model_chat_template(model_, prompt);

        if (debug_mode_) {
            print_vram_free_debug();
        }

        // トークン化
        std::vector<llama_token> prompt_tokens = common_tokenize(ctx_, model_input, true);

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

    // Gemma 4 thinking形式が混在した場合は、final channel側のみを返す。
    const std::string thought_open = "<|channel|>thought";
    size_t thought_pos = result.find(thought_open);
    if (thought_pos != std::string::npos) {
        size_t thought_close = result.find("<|channel|>", thought_pos + thought_open.size());
        if (thought_close != std::string::npos) {
            result.erase(thought_pos, (thought_close - thought_pos) + std::string("<|channel|>").size());
        } else {
            result.erase(thought_pos);
        }
    }

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
