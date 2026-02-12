#pragma once

#include <string>

// 互換用の最小スタブ（本体は archive に退避）
class PromptManager {
public:
    PromptManager(int /*ctx_limit*/ = 2048) {}

    void set_system(const std::string& /*text*/) {}
    void set_context(const std::string& /*context*/) {}
    void set_max_tokens(int /*tokens*/) {}

    void clear_system() {}
    void clear_context() {}

    void add_to_history(const std::string& /*role*/, const std::string& /*content*/) {}
    void clear_history() {}
    void limit_history(int /*max_turns*/) {}

    std::string build_final(const std::string& user_input) { return user_input; }

    int get_current_length() const { return 0; }
    std::string get_system_prompt() const { return ""; }
    std::string get_context() const { return ""; }
    int get_history_size() const { return 0; }
};
