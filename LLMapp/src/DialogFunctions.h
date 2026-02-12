#ifndef DIALOG_FUNCTIONS_H
#define DIALOG_FUNCTIONS_H

#include "LLMInference.h"
#include <string>

// ===== コンソール初期化関数 =====
void initialize_console();

// ===== LLM初期化関数 =====
bool create_and_initialize_llm(LLMInference& llm);

// ===== 単発推論関数 =====
std::string run_single_inference(LLMInference& llm, const std::string& user_input);

#endif // DIALOG_FUNCTIONS_H
