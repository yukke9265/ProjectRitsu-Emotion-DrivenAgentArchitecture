#include "LLMInference.h"
#include "DialogFunctions.h"
#include <iostream>
#include <string>

int main(int /*argc*/, char** /*argv*/) {
    // ===== 初期化 =====
    initialize_console();

    // LLMの作成と初期化
    LLMInference llm(
        "D:/0_OllamaModels/WS/models/LFM2.5-1.2B-Instruct-BF16.gguf",
        99,    // GPU layers
        8192,  // context size
        -1     // n_predict
    );

    if (!create_and_initialize_llm(llm)) {
        return 1;
    }

    // ===== 単発推論 =====
    std::cout << "Input: ";
    std::cout.flush();

    std::string user_input;
    std::getline(std::cin, user_input);

    if (user_input.empty()) {
        std::cout << "入力が空のため終了します。" << std::endl;
        return 0;
    }

    std::string result = run_single_inference(llm, user_input);
    if (!result.empty()) {
        std::cout << "Output: " << result << std::endl;
    }

    return 0;
}