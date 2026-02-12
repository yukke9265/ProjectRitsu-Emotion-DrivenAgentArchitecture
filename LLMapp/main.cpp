#include "LLMInference.h"
#include "DialogFunctions.h"
#include "Config.h"
#include <iostream>
#include <string>

int main(int /*argc*/, char** /*argv*/) {
    // ===== 初期化 =====
    initialize_console();

    // LLMの作成と初期化
    LLMInference llm(
        DEFAULT_MODEL_PATH,
        DEFAULT_GPU_LAYERS,
        DEFAULT_CONTEXT_SIZE,
        DEFAULT_N_PREDICT
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