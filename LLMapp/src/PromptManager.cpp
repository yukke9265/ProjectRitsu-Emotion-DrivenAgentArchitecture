#include "PromptManager.h"

PromptManager::PromptManager(int ctx_limit)
    : max_tokens_(ctx_limit) {
}

void PromptManager::set_system(const std::string& text) {
    system_prompt_ = "<|im_start|>system\n" + text + "<|im_end|>\n";
}

void PromptManager::set_context(const std::string& context) {
    search_context_ = "### 関連情報\n" + context + "\n";
}

void PromptManager::add_to_history(const std::string& role, const std::string& content) {
    conversation_history_.push_back({role, content});
}

void PromptManager::limit_history(int max_turns) {
    // 履歴を直近max_turns個に制限（1turn = user + assistant の2つ）
    int max_entries = max_turns * 2;  // 1ターン = user + assistant で2エントリ
    
    if (static_cast<int>(conversation_history_.size()) > max_entries) {
        // 最初のエントリを削除（古い履歴を削除）
        conversation_history_.erase(
            conversation_history_.begin(),
            conversation_history_.begin() + (conversation_history_.size() - max_entries)
        );
    }
}

int PromptManager::get_current_length() const {
    return static_cast<int>(system_prompt_.length() + search_context_.length());
}

std::string PromptManager::build_final(const std::string& user_input) {
    // システムプロンプト + 会話履歴 + ユーザー入力の組み立て
    std::string final_prompt = system_prompt_;
    
    // 会話履歴を追加
    for (const auto& msg : conversation_history_) {
        final_prompt += "<|im_start|>" + msg.first + "\n";
        final_prompt += msg.second;
        final_prompt += "<|im_end|>\n";
    }

    // 新しいユーザー入力を追加
    final_prompt += "<|im_start|>user\n";
    
    // 粗い推定：1トークン ≈ 4文字
    int estimated_tokens = static_cast<int>(
        (final_prompt.length() + user_input.length()) / 4
    );

    // コンテキストを含める場合、トークン数をチェック
    if (estimated_tokens + (search_context_.length() / 4) < max_tokens_) {
        final_prompt += search_context_;
    }
    // トークン数が超過する場合は検索コンテキストを削る
    // （ユーザー入力とシステムプロンプトは優先度を高く保つ）

    final_prompt += user_input;
    final_prompt += "<|im_end|>\n<|im_start|>assistant\n";

    //printf("\n[debug]\ninputprommpt %s[debug_end]\n", final_prompt.c_str());

    return final_prompt;
}

