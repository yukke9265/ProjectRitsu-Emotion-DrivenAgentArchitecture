// Archived on 2026-02-11 from src/DialogFunctions.h
#ifndef DIALOG_FUNCTIONS_ARCHIVE_H
#define DIALOG_FUNCTIONS_ARCHIVE_H

#include "LLMInference.h"
#include "PromptManager_archive.h"
#include <string>

// ===== ファイルから初期入力を読み込む関数 =====
std::string load_initial_input_from_file(const std::string& directory = "TestTxt");

// ===== コンソール初期化関数 =====
void initialize_console();

// ===== LLM初期化関数 =====
bool create_and_initialize_llm(LLMInference& llm);

// ===== PromptManager作成・設定関数 =====
void create_prompt_managers(PromptManager& pmA, PromptManager& pmB);

// ===== 文字列トリム関数 =====
std::string trim_string(const std::string& str);

// ===== 2段階推論処理関数 =====
bool process_two_stage_inference(
    LLMInference& llm,
    PromptManager& pmA,
    PromptManager& pmB,
    const std::string& user_input,
    std::string& final_output
);

// ===== ユーザー入力取得関数 =====
std::string get_user_input(bool& first_iteration, const std::string& initial_input);

// ===== 対話ループ関数 =====
void run_interactive_dialog(
    LLMInference& llm,
    PromptManager& pmA,
    PromptManager& pmB,
    const std::string& initial_input,
    const std::string& system_prompt_a,
    const std::string& system_prompt_b
);

// ===== 交互対話ループ関数 =====
void run_alternating_dialog(
    LLMInference& llm,
    PromptManager& pmA,
    PromptManager& pmB,
    const std::string& initial_input,
    int num_iterations,
    const std::string& system_prompt_a,
    const std::string& system_prompt_b
);

// ===== 自動対話ループ関数 =====
void run_autonomous_alternating_dialog(
    LLMInference& llm,
    PromptManager& pmA,
    PromptManager& pmB,
    const std::string& initial_input,
    int num_iterations,
    const std::string& system_prompt_a,
    const std::string& system_prompt_b
);

#endif // DIALOG_FUNCTIONS_ARCHIVE_H
