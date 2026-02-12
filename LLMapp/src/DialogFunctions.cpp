#include "DialogFunctions.h"
#include <iostream>
#include <windows.h>
#include <locale>

// ===== コンソール初期化関数 =====
void initialize_console() {
    // 1. コンソールの入出力を UTF-8 (65001) に設定
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 2. C++ の標準入出力ストリームに UTF-8 ロケールを適用
    try {
        std::locale::global(std::locale(".UTF8"));
    } catch (...) {
        std::locale::global(std::locale(""));
    }
    std::cin.imbue(std::locale());
    std::cout.imbue(std::locale());

    // 3. 標準入出力との同期を有効に保つ
    std::ios::sync_with_stdio(true);
}

// ===== LLM初期化関数 =====
bool create_and_initialize_llm(LLMInference& llm) {
    if (!llm.initialize()) {
        std::cerr << "[ERROR] Initialization failed: " << llm.get_last_error() << std::endl;
        return false;
    }
    return true;
}

// ===== 単発推論関数 =====
std::string run_single_inference(LLMInference& llm, const std::string& user_input) {
    if (user_input.empty()) {
        return "";
    }

    std::string result = llm.infer(user_input);
    if (result.empty()) {
        std::cerr << "[ERROR] 推論エラー: " << llm.get_last_error() << std::endl;
    }
    return result;
}
