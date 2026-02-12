// Archived on 2026-02-11 from src/PromptManager.h
#pragma once

#include <string>
#include <vector>
#include <utility>

class PromptManager {
private:
    std::string system_prompt_;
    std::string search_context_;
    int max_tokens_;
    // 会話履歴: (role, content) のペア
    std::vector<std::pair<std::string, std::string>> conversation_history_;

public:
    // コンストラクタ
    PromptManager(int ctx_limit = 2048);

    // システム命令を設定
    void set_system(const std::string& text);

    // 検索結果などを注入
    void set_context(const std::string& context);

    // max_tokensを設定
    void set_max_tokens(int tokens) { max_tokens_ = tokens; }

    // システムプロンプトをクリア
    void clear_system() { system_prompt_.clear(); }

    // コンテキストをクリア
    void clear_context() { search_context_.clear(); }

    // 会話履歴に追加（ユーザーまたはアシスタントの回答）
    void add_to_history(const std::string& role, const std::string& content);

    // 会話履歴をクリア
    void clear_history() { conversation_history_.clear(); }

    // 会話履歴を制限（直近max_turnsのターンのみを保持）
    void limit_history(int max_turns);

    // 最終的なプロンプトを組み立てる（会話履歴を含む）
    std::string build_final(const std::string& user_input);

    // 現在の設定状態を取得
    int get_current_length() const;
    std::string get_system_prompt() const { return system_prompt_; }
    std::string get_context() const { return search_context_; }
    int get_history_size() const { return static_cast<int>(conversation_history_.size()); }
};
