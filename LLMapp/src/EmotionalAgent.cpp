#include "EmotionalAgent.h"
#include "AgentStatePersistence.h"
#include "Config.h"
#include <algorithm> // 追加
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace {
std::string trim_and_lower_copy(std::string text) {
    auto is_ws = [](unsigned char c) { return std::isspace(c) != 0; };

    auto first = std::find_if_not(text.begin(), text.end(), is_ws);
    if (first == text.end()) {
        return "";
    }
    auto last = std::find_if_not(text.rbegin(), text.rend(), is_ws).base();
    text = std::string(first, last);

    if (text.size() >= 2) {
        const char front = text.front();
        const char back = text.back();
        const bool double_quoted = (front == '"' && back == '"');
        const bool single_quoted = (front == '\'' && back == '\'');
        if (double_quoted || single_quoted) {
            text = text.substr(1, text.size() - 2);
            first = std::find_if_not(text.begin(), text.end(), is_ws);
            if (first == text.end()) {
                return "";
            }
            last = std::find_if_not(text.rbegin(), text.rend(), is_ws).base();
            text = std::string(first, last);
        }
    }

    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return text;
}

std::string normalize_control_command(const std::string& input) {
    const std::string token = trim_and_lower_copy(input);

    if (token == "exit" || token == "quit" || token == "debug" ||
        token == "emotion" || token == "history" || token == "reset" ||
        token == "prompt") {
        return token;
    }

    std::string alpha_only;
    alpha_only.reserve(token.size());
    for (unsigned char c : token) {
        if (std::isalpha(c)) {
            alpha_only.push_back(static_cast<char>(std::tolower(c)));
        }
    }

    if (alpha_only == "exit" || alpha_only == "quit" || alpha_only == "debug" ||
        alpha_only == "emotion" || alpha_only == "history" || alpha_only == "reset" ||
        alpha_only == "prompt") {
        return alpha_only;
    }

    return token;
}

std::string trim_fixed_prompt_head_for_debug(const std::string& prompt) {
    static const std::string marker = "# 現在のあなたの感情状態と応答トーン";
    const auto marker_pos = prompt.find(marker);
    if (marker_pos == std::string::npos) {
        return prompt;
    }

    std::string result;
    result.reserve(prompt.size());
    result += "[debug表示用: 固定システムプロンプト先頭を省略]\n\n";
    result += prompt.substr(marker_pos);
    return result;
}

std::string get_runtime_log_file_from_env() {
    const char* value = std::getenv("LLMAPP_ONE_SHOT_RUNTIME_LOG");
    if (!value) {
        return "";
    }
    return trim_and_lower_copy(value).empty() ? "" : std::string(value);
}

bool is_env_flag_enabled(const char* env_name) {
    const char* value = std::getenv(env_name);
    if (!value) {
        return false;
    }

    std::string token = trim_and_lower_copy(std::string(value));
    return token == "1" || token == "true" || token == "yes" || token == "on";
}

bool is_tool_phase_only_mode_enabled() {
    return is_env_flag_enabled("LLMAPP_TOOL_PHASE_ONLY");
}

bool is_response_phase_only_mode_enabled() {
    return is_env_flag_enabled("LLMAPP_RESPONSE_PHASE_ONLY");
}

void append_raw_output_to_runtime_log(
    const std::string& phase_name,
    int phase_index,
    const std::string& raw_output) {

    const std::string log_file = get_runtime_log_file_from_env();
    if (log_file.empty()) {
        return;
    }

    std::ofstream ofs(log_file, std::ios::binary | std::ios::app);
    if (!ofs.is_open()) {
        return;
    }

    ofs << "[raw_output_begin] phase=" << phase_name;
    if (phase_index >= 0) {
        ofs << " index=" << phase_index;
    }
    ofs << "\n";
    ofs << raw_output << "\n";
    ofs << "[raw_output_end] phase=" << phase_name;
    if (phase_index >= 0) {
        ofs << " index=" << phase_index;
    }
    ofs << "\n";
}
}

EmotionalAgent::EmotionalAgent(
    const std::string& model_path,
    const PersonalityConstitution& constitution)
    : model_path_(model_path)
    , initialized_(false)
    , last_error_("")
    , debug_mode_(false)
    , tool_use_enabled_(true)
    , max_tool_iterations_(3)
    , tool_executor_(nullptr) {
    
    // モジュールの初期化
    input_analyzer_ = std::make_unique<InputAnalyzer>();
    tool_analyzer_ = std::make_unique<ToolAnalyzer>();
    emotion_engine_ = std::make_unique<EmotionEngine>(constitution);
    memory_controller_ = std::make_unique<MemoryController>(10);  // 短期メモリ10ターン
    prompt_orchestrator_ = std::make_unique<PromptOrchestrator>();
    
    // LLMInferenceは後で初期化（initialize()で）
    llm_inference_ = nullptr;
}

EmotionalAgent::~EmotionalAgent() {
}

bool EmotionalAgent::initialize() {
    if (initialized_) {
        return true;
    }

    // LLMの初期化
    llm_inference_ = std::make_unique<LLMInference>(
        model_path_,
        DEFAULT_GPU_LAYERS,
        DEFAULT_CONTEXT_SIZE,
        DEFAULT_N_PREDICT
    );

    if (!llm_inference_->initialize()) {
        last_error_ = "LLM初期化失敗: " + llm_inference_->get_last_error();
        return false;
    }

    // InputAnalyzerにLLMインスタンスを共有（シングルLLMインスタンス）
    input_analyzer_->set_llm_inference(llm_inference_.get());
    
    // ToolAnalyzerにもLLMインスタンスを共有
    tool_analyzer_->set_llm_inference(llm_inference_.get());
    
    // LLMベースの分析モードを有効化（ハイブリッドモード）
    input_analyzer_->enable_llm_mode(true);

    initialized_ = true;
    return true;
}

std::string EmotionalAgent::process(const std::string& user_input) {
    if (!initialized_) {
        return "[エラー] エージェントが初期化されていません。";
    }

    const std::string command = normalize_control_command(user_input);
    const bool has_meaningful_input = !trim_and_lower_copy(user_input).empty();
    const bool tool_phase_only_mode = is_tool_phase_only_mode_enabled();
    const bool response_phase_only_mode = is_response_phase_only_mode_enabled();

    if (command == "exit" || command == "quit" ||
        command == "debug" || command == "emotion" ||
        command == "history" || command == "reset") {
        return "";
    }

    // 空入力は「初回起動の挨拶生成」時のみ許可。
    // 既に会話履歴がある場合は no-op として扱い、解析/記憶更新を行わない。
    if (!has_meaningful_input &&
        memory_controller_ &&
        memory_controller_->get_short_term_size() > 0) {
        return "";
    }

    try {
        // ターン開始時に構造化システムログをクリア
        clear_system_log_sections();

        const bool is_initial_greeting_turn =
            !has_meaningful_input &&
            memory_controller_ &&
            memory_controller_->get_short_term_size() == 0;

        if (is_initial_greeting_turn && !tool_phase_only_mode && !response_phase_only_mode) {
            add_system_log_section(
                "Startup",
                "初回起動ターンです。ユーザー入力はありません。最初の挨拶を自然に1回だけ生成してください。"
            );
        }

        bool allow_tool_call = false;

        if (!response_phase_only_mode && tool_use_enabled_ && tool_executor_) {
            const auto tools = tool_executor_->list_tools();
            if (!tools.empty()) {
                allow_tool_call = true;
                add_system_log_section("Tool Interface", ToolIOProtocol::build_tool_guide(tools));

                const auto has_number_guess_tool = std::any_of(
                    tools.begin(),
                    tools.end(),
                    [](const ToolSpec& spec) {
                        return spec.name == "number_guess_game";
                    }
                );

                if (has_number_guess_tool) {
                    add_system_log_section(
                        "Game Tool Policy",
                        "ユーザーが数当てゲームを希望した場合、推測や判定を文章で決め打ちせず"
                        " number_guess_game を使って進行してください。"
                        "開始時は action=start、進行中の判定は action=guess、"
                        "状態確認は action=status、終了/やり直しは action=reset を優先してください。"
                        "数値の正誤判定は必ずツール結果に従ってください。"
                    );
                }
            }
        }

        if (debug_mode_) {
            std::cout << "\n" << std::string(60, '=') << "\n";
            std::cout << "  デバッグモード: 処理開始\n";
            std::cout << std::string(60, '=') << "\n";
            std::cout << "ユーザー入力: \"" << user_input << "\"\n\n";
        }

        // ===== 処理フロー =====

        // Step 1: 入力解析（会話履歴を渡す）
        if (debug_mode_) {
            std::cout << "--- [Step 1] 入力解析 ---\n";
        }
        
        const auto& conversation_history = memory_controller_->get_short_term_history();
        AnalyzedInput analyzed;

        if (is_initial_greeting_turn && !tool_phase_only_mode && !response_phase_only_mode) {
            analyzed.raw_text = "";
            analyzed.topic = "session_start";
            analyzed.intent = Intent::GREETING;
            analyzed.evaluation_to_ai = EvaluationToAI::NEUTRAL;
            analyzed.sentiment_score = 0.2;
            analyzed.keywords = { "起動", "初回", "挨拶" };

            if (debug_mode_) {
                std::cout << "会話履歴（0ターン）: 初回起動のため入力解析をスキップ\n\n";
            }
        } else {
            if (debug_mode_ && !conversation_history.empty()) {
                std::cout << "会話履歴（" << conversation_history.size() << "ターン）:\n";
                int count = 0;
                for (const auto& turn : conversation_history) {
                    std::cout << "  [" << ++count << "] " << turn.role << ": "
                              << (turn.content.length() > 60 ? turn.content.substr(0, 60) + "..." : turn.content) << "\n";
                }
                std::cout << "\n";
            }

            analyzed = input_analyzer_->analyze(user_input, &conversation_history);
        }
        
        if (debug_mode_) {
            std::cout << "解析結果:\n";
            std::cout << "  Topic: " << analyzed.topic << "\n";
            std::cout << "  Intent: " << intent_to_string(analyzed.intent) << "\n";
            std::cout << "  Evaluation: " << evaluation_to_string(analyzed.evaluation_to_ai) << "\n";
            std::cout << "  Sentiment: " << analyzed.sentiment_score << "\n";
            std::cout << "  Keywords: ";
            for (size_t i = 0; i < analyzed.keywords.size(); ++i) {
                std::cout << analyzed.keywords[i];
                if (i < analyzed.keywords.size() - 1) std::cout << ", ";
            }
            std::cout << "\n\n";
        }

        // Step 2: 感情エンジンで評価・更新
        if (debug_mode_) {
            std::cout << "--- [Step 2] 感情エンジン ---\n";
            std::cout << "更新前の感情状態:\n" << emotion_engine_->describe_emotion() << "\n";
        }
        
        emotion_engine_->appraise_and_update(analyzed);
        emotion_engine_->apply_decay();  // 時間経過による減衰
        
        if (debug_mode_) {
            std::cout << "更新後の感情状態:\n" << emotion_engine_->describe_emotion() << "\n\n";
        }

        // Step 3: 短期メモリに追加
        if (debug_mode_) {
            std::cout << "--- [Step 3] 短期メモリ更新 ---\n";
            std::cout << "ユーザー入力を短期メモリに追加\n\n";
        }
        
        if (has_meaningful_input) {
            memory_controller_->add_to_short_term("user", user_input);
        }

        // Step 4: プロンプト生成
        if (debug_mode_) {
            std::cout << "--- [Step 4] プロンプト生成 ---\n";
        }
        
        const std::string tool_base_prompt = prompt_orchestrator_->build_final_prompt(
            user_input,
            *emotion_engine_,
            *memory_controller_,
            PromptOrchestrator::PromptPhase::Tool
        );

        const std::string response_base_prompt = prompt_orchestrator_->build_final_prompt(
            user_input,
            *emotion_engine_,
            *memory_controller_,
            PromptOrchestrator::PromptPhase::Response
        );
        
        if (debug_mode_) {
            std::cout << "生成されたTool Phaseベースプロンプト:\n";
            std::cout << std::string(60, '-') << "\n";
            std::cout << trim_fixed_prompt_head_for_debug(tool_base_prompt) << "\n";
            std::cout << std::string(60, '-') << "\n\n";
            std::cout << "生成されたResponse Phaseベースプロンプト:\n";
            std::cout << std::string(60, '-') << "\n";
            std::cout << trim_fixed_prompt_head_for_debug(response_base_prompt) << "\n";
            std::cout << std::string(60, '-') << "\n\n";
        }

        // Step 5: LLMで応答生成
        // ドキュメント運用方針:
        // - このブロックのコメントを「運用仕様の正本」とする（READMEは要約のみ）。
        // - 1ターンは Tool Phase → Response Phase の順で固定する。
        // - Tool Phase は <tool_call> のみ許可し、不要時は __no_tool__ を返す。
        // - Response Phase は <assistant_response> を優先し、失敗時のみ自然文フォールバックを許可する。
        // - 仕様を変える場合は、このコメントと ToolIOProtocol の契約生成を同時に更新する。
        if (debug_mode_) {
            std::cout << "--- [Step 5] LLM推論 ---\n";
            std::cout << "LLMに推論を要求中...\n";
        }

        std::string response;
        std::string tool_feedback_blocks;
        const std::string no_tool_call_name = "finish_tool_planning";

        auto preview_text = [](const std::string& text, size_t max_len) {
            if (text.size() <= max_len) {
                return text;
            }
            return text.substr(0, max_len) + "...";
        };

        auto has_tool_markup = [](const std::string& text) {
            return text.find("<tool_call>") != std::string::npos ||
                text.find("</tool_call>") != std::string::npos;
        };

        auto looks_like_contract_or_instruction_echo = [](const std::string& text) {
            const std::string lowered = trim_and_lower_copy(text);
            if (lowered.empty()) {
                return false;
            }

            const bool has_contract_keywords =
                lowered.find("assistant_channel") != std::string::npos ||
                lowered.find("tool_channel") != std::string::npos ||
                lowered.find("assistant_response") != std::string::npos ||
                lowered.find("tool_call") != std::string::npos ||
                lowered.find("出力契約") != std::string::npos ||
                lowered.find("禁止") != std::string::npos ||
                lowered.find("見出し") != std::string::npos ||
                lowered.find("注意書き") != std::string::npos;

            std::istringstream iss(lowered);
            std::string line;
            int non_empty_lines = 0;
            int bullet_lines = 0;
            while (std::getline(iss, line)) {
                const std::string t = trim_and_lower_copy(line);
                if (t.empty()) {
                    continue;
                }
                ++non_empty_lines;
                if (t.rfind("-", 0) == 0 || t.rfind("*", 0) == 0) {
                    ++bullet_lines;
                }
            }

            const bool mostly_bullets = non_empty_lines >= 2 && bullet_lines == non_empty_lines;
            return has_contract_keywords || mostly_bullets;
        };

        // Tool Phase: ツール計画・実行専用（最終ユーザー応答は生成しない）
        // needed_tools パラメータで、フィルタリング済みのツール情報を受け取る
        auto build_tool_phase_prompt = [&](bool strict_retry_mode, const std::vector<ToolSpec>* needed_tools = nullptr) {
            std::string phase_prompt = tool_base_prompt;
            
            // ツール情報を挿入（フィルタリング済みの場合のみ）
            if (needed_tools && !needed_tools->empty()) {
                phase_prompt += "\n\n## このターンで必要と判定されたツール候補\n\n";
                for (const auto& tool : *needed_tools) {
                    phase_prompt += "\n- " + tool.name + ": " + tool.description + "\n";
                    if (!tool.input_schema.empty()) {
                        phase_prompt += "  使い方: " + tool.input_schema + "\n";
                    } else {
                        phase_prompt += "  使い方: help を tool_call して {\"tool\":\"" + tool.name + "\"} を確認\n";
                    }
                }
                phase_prompt += "\n";
            }

            if (!tool_feedback_blocks.empty()) {
                phase_prompt += "\n# ツール実行ログ\n\n";
                phase_prompt += tool_feedback_blocks;
                phase_prompt += "\n";
            }

            if (strict_retry_mode) {
                phase_prompt +=
                    "\n【再出力指示】<tool_call> ブロック1つだけを出力してください。"
                    "前置き・説明文・箇条書き・Markdownコードブロック（```）は禁止です。\n";
            }

            return phase_prompt;
        };

        // Response Phase: 最終応答専用（tool_call は禁止）
        auto build_response_phase_prompt = [&](bool strict_retry_mode) {
            std::string phase_prompt = response_base_prompt;

            if (!tool_feedback_blocks.empty()) {
                phase_prompt += "# ツール実行ログ\n\n";
                phase_prompt += tool_feedback_blocks;
                phase_prompt += "\n\n";
            }

            if (strict_retry_mode) {
                phase_prompt +=
                    "【再出力指示】可能なら <assistant_response> ブロック1つのみで再出力してください。"
                    "自然文のみでも可ですが、前置きや契約文は含めないでください。\n";
            }

            return phase_prompt;
        };

        if (tool_phase_only_mode) {
            if (!allow_tool_call) {
                response = "[Tool Phase専用モード] ツール実行環境が利用できません。";
            } else {
                const auto tools = tool_executor_->list_tools();
                int tool_contract_retry_budget = 1;
                bool strict_retry_mode = false;

                for (int tool_iter = 0; tool_iter < std::max(1, max_tool_iterations_); ++tool_iter) {
                    const std::string tool_phase_prompt = build_tool_phase_prompt(strict_retry_mode, &tools);
                    strict_retry_mode = false;

                    const std::string raw_output = llm_inference_->infer_raw(tool_phase_prompt);
                    append_raw_output_to_runtime_log("tool-only", tool_iter + 1, raw_output);

                    ToolCall tool_call;
                    if (ToolIOProtocol::try_parse_tool_call_strict(raw_output, tool_call)) {
                        std::ostringstream oss;
                        oss << "<tool_call>\n";
                        oss << "name: " << tool_call.name << "\n";
                        oss << "input:\n";
                        oss << tool_call.input << "\n";
                        oss << "</tool_call>";
                        response = oss.str();
                        break;
                    }

                    if (tool_contract_retry_budget > 0) {
                        --tool_contract_retry_budget;
                        strict_retry_mode = true;
                        continue;
                    }

                    response = "ごめん、出力形式が崩れたので返答を作り直すね。もう一度だけ同じ内容を送って。";
                    break;
                }
            }

            if (debug_mode_) {
                std::cout << "--- [Tool Phase Only] 応答 ---\n";
                std::cout << std::string(60, '-') << "\n";
                std::cout << response << "\n";
                std::cout << std::string(60, '-') << "\n\n";
            }

            memory_controller_->add_to_short_term("assistant", response);
            consolidate_memories();
            return response;
        }

        if (allow_tool_call && !response_phase_only_mode) {
            // Step 5a: ツール分析（このターンで必要なツールを判定）
            if (debug_mode_) {
                std::cout << "--- [Tool Analysis] 開始 ---\n";
                std::cout << "このターンで必要なツールを分析中...\n\n";
            }

            const auto tools = tool_executor_->list_tools();
            const auto analyzed_tools = tool_analyzer_->analyze(user_input, tools, &conversation_history);

            auto build_needed_tools = [&](const AnalyzedToolSet& analysis, bool prefer_finish_only) {
                std::vector<ToolSpec> selected;

                auto add_tool_by_name = [&](const std::string& tool_name) {
                    auto it = std::find_if(tools.begin(), tools.end(),
                        [&tool_name](const ToolSpec& spec) { return spec.name == tool_name; });
                    if (it == tools.end()) {
                        return;
                    }

                    const bool already_added = std::any_of(selected.begin(), selected.end(),
                        [&tool_name](const ToolSpec& spec) { return spec.name == tool_name; });
                    if (!already_added) {
                        selected.push_back(*it);
                    }
                };

                if (analysis.needs_tool_call) {
                    for (const auto& tool_name : analysis.tool_names) {
                        add_tool_by_name(tool_name);
                    }
                }

                if (prefer_finish_only) {
                    bool has_non_finish = false;
                    for (const auto& spec : selected) {
                        if (spec.name != "finish_tool_planning") {
                            has_non_finish = true;
                            break;
                        }
                    }

                    if (!has_non_finish) {
                        selected.clear();
                        add_tool_by_name("finish_tool_planning");
                        return selected;
                    }
                }

                add_tool_by_name("finish_tool_planning");
                return selected;
            };

            if (debug_mode_) {
                if (analyzed_tools.needs_tool_call) {
                    std::cout << "[ToolAnalyzer] " << analyzed_tools.reason << "\n";
                    std::cout << "[ToolAnalyzer] 必要なツール: ";
                    for (size_t i = 0; i < analyzed_tools.tool_names.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << analyzed_tools.tool_names[i];
                    }
                    std::cout << "\n\n";
                } else {
                    std::cout << "[ToolAnalyzer] " << analyzed_tools.reason << "\n\n";
                }
            }

            std::vector<ToolSpec> needed_tools;

            // ツール不要と判定された場合、Tool Phase をスキップ
            if (!analyzed_tools.needs_tool_call) {
                if (debug_mode_) {
                    std::cout << "[Tool Phase] スキップ（ツール不要）\n";
                    std::cout << "--- Tool Phase 完了 ---\n";
                    std::cout << "ツール実行なし\n\n";
                }
                // tool_feedback_blocks は空のまま（Response Phase へ直進）
            } else {
                // Step 5b: Tool Phase（フィルタリングされたツール情報で実行）
                needed_tools = build_needed_tools(analyzed_tools, false);

                if (debug_mode_) {
                    std::cout << "--- [Tool Phase] 開始（フィルタリング済みツール情報を使用）---\n";
                    std::cout << "対象ツール数: " << needed_tools.size() << "\n\n";
                }
                int tool_contract_retry_budget = 1;
                bool strict_retry_mode = false;

                for (int tool_iter = 0; tool_iter < max_tool_iterations_; ++tool_iter) {
                    const std::string tool_phase_prompt = build_tool_phase_prompt(strict_retry_mode, &needed_tools);
                    strict_retry_mode = false;

                    if (debug_mode_) {
                        std::cout << "--- [Tool Phase " << (tool_iter + 1) << "] プロンプト送信 ---\n";
                        std::cout << std::string(60, '-') << "\n";
                        std::cout << trim_fixed_prompt_head_for_debug(tool_phase_prompt) << "\n";
                        std::cout << std::string(60, '-') << "\n\n";
                    }

                    const std::string raw_output = llm_inference_->infer_raw(tool_phase_prompt);
                    append_raw_output_to_runtime_log("tool", tool_iter + 1, raw_output);

                    if (debug_mode_) {
                        std::cout << "--- [Tool Phase " << (tool_iter + 1) << "] LLM出力 ---\n";
                        std::cout << std::string(60, '-') << "\n";
                        std::cout << raw_output << "\n";
                        std::cout << std::string(60, '-') << "\n\n";
                    }

                    ToolCall tool_call;
                    const bool has_tool_call = ToolIOProtocol::try_parse_tool_call_strict(raw_output, tool_call);
                    if (!has_tool_call) {
                        if (debug_mode_) {
                            std::cout << "[ToolPhaseViolation] invalid_tool_call_format\n";
                            std::cout << "[ToolPhaseViolation] raw_preview=" << preview_text(raw_output, 600) << "\n";
                        }

                        if (tool_contract_retry_budget > 0) {
                            --tool_contract_retry_budget;
                            strict_retry_mode = true;
                            if (debug_mode_) {
                                std::cout << "[ToolPhase] リトライします（残り予算: " << tool_contract_retry_budget << "）\n\n";
                            }
                            continue;
                        }

                        if (debug_mode_) {
                            std::cout << "[ToolPhase] リトライ予算を使い切りました。ツール実行なしで Response Phase へ移行します。\n\n";
                        }
                        break;
                    }

                    if (tool_call.name == no_tool_call_name) {
                        if (debug_mode_) {
                            std::cout << "[ToolPhase] finish_tool_planning called (ツール実行を終了し、Response Phaseへ移行)\n\n";
                        }
                        break;
                    }

                    if (debug_mode_) {
                        std::cout << "[ToolCall] name=" << tool_call.name << "\n";
                        std::cout << "[ToolCall] input=" << tool_call.input << "\n";
                    }

                    ToolResult tool_result = tool_executor_->execute(tool_call);
                    const std::string result_block = ToolIOProtocol::build_tool_result_block(tool_call, tool_result);

                    if (!tool_feedback_blocks.empty()) {
                        tool_feedback_blocks += "\n\n";
                    }
                    tool_feedback_blocks += result_block;

                    if (debug_mode_) {
                        std::cout << "[ToolResult] success=" << (tool_result.success ? "true" : "false") << "\n";
                        if (tool_result.success) {
                            std::cout << "[ToolResult] output=" << tool_result.output << "\n";
                        }
                        else {
                            std::cout << "[ToolResult] error=" << tool_result.error << "\n";
                        }
                    }

                    // ツール成功後は再分析し、次に提示するツール候補を更新する
                    if (tool_result.success) {
                        std::string reanalyze_input = user_input;
                        reanalyze_input += "\n\n[直前のツール実行結果]\n";
                        reanalyze_input += result_block;
                        reanalyze_input += "\n\n追加ツールが不要なら finish_tool_planning のみを推奨してください。";

                        const auto post_tool_analysis = tool_analyzer_->analyze(
                            reanalyze_input,
                            tools,
                            &conversation_history
                        );

                        needed_tools = build_needed_tools(post_tool_analysis, true);

                        if (debug_mode_) {
                            std::cout << "[ToolAnalyzer/PostTool] " << post_tool_analysis.reason << "\n";
                            std::cout << "[ToolAnalyzer/PostTool] 次候補ツール: ";
                            for (size_t i = 0; i < needed_tools.size(); ++i) {
                                if (i > 0) std::cout << ", ";
                                std::cout << needed_tools[i].name;
                            }
                            std::cout << "\n\n";
                        }
                    }
                }
            }
        }

        if (debug_mode_) {
            std::cout << "--- Tool Phase 完了 ---\n";
            if (tool_feedback_blocks.empty()) {
                std::cout << "ツール実行なし\n\n";
            } else {
                std::cout << "ツール実行ログあり\n\n";
            }
        }

        int response_contract_retry_budget = 1;
        bool strict_response_retry_mode = false;

        while (true) {
            const std::string response_phase_prompt = build_response_phase_prompt(strict_response_retry_mode);
            strict_response_retry_mode = false;
            
            if (debug_mode_) {
                std::cout << "--- [Response Phase] プロンプト送信 ---\n";
                std::cout << std::string(60, '-') << "\n";
                std::cout << trim_fixed_prompt_head_for_debug(response_phase_prompt) << "\n";
                std::cout << std::string(60, '-') << "\n\n";
            }
            
            const std::string raw_output = llm_inference_->infer_raw(response_phase_prompt);
            append_raw_output_to_runtime_log("response", -1, raw_output);
            
            if (debug_mode_) {
                std::cout << "--- [Response Phase] LLM出力 ---\n";
                std::cout << std::string(60, '-') << "\n";
                std::cout << raw_output << "\n";
                std::cout << std::string(60, '-') << "\n\n";
            }

            std::string assistant_response;
            if (ToolIOProtocol::try_parse_assistant_response_strict(raw_output, assistant_response)) {
                response = assistant_response;
                break;
            }

            const std::string cleaned = LLMInference::cleanup_response(raw_output);
            if (!cleaned.empty() &&
                !has_tool_markup(raw_output) &&
                !looks_like_contract_or_instruction_echo(cleaned)) {
                if (debug_mode_) {
                    std::cout << "[ResponsePhaseFallback] untagged_natural_response_accepted\n";
                }
                response = cleaned;
                break;
            }

            if (debug_mode_) {
                std::cout << "[ResponsePhaseViolation] invalid_assistant_response_format\n";
                std::cout << "[ResponsePhaseViolation] raw_preview=" << preview_text(raw_output, 600) << "\n";
            }

            if (response_contract_retry_budget > 0) {
                --response_contract_retry_budget;
                strict_response_retry_mode = true;
                continue;
            }

            response = "ごめん、出力形式が崩れたので返答を作り直すね。もう一度だけ同じ内容を送って。";
            break;
        }
        
        if (debug_mode_) {
            std::cout << "LLM応答:\n";
            std::cout << std::string(60, '-') << "\n";
            std::cout << response << "\n";
            std::cout << std::string(60, '-') << "\n\n";
        }

        // Step 6: 応答を短期メモリに追加
        if (debug_mode_) {
            std::cout << "--- [Step 6] 応答を記憶 ---\n";
            std::cout << "AI応答を短期メモリに追加\n\n";
        }
        
        memory_controller_->add_to_short_term("assistant", response);

        // Step 7: 定期的に記憶を統合（簡易実装：毎回実行）
        consolidate_memories();
        
        if (debug_mode_) {
            std::cout << "--- [Step 7] 記憶統合 ---\n";
            std::cout << "短期メモリサイズ: " << memory_controller_->get_short_term_size() << "\n";
            std::cout << "長期メモリサイズ: " << get_episode_count() << "\n";
            std::cout << "\n" << std::string(60, '=') << "\n";
            std::cout << "  処理完了\n";
            std::cout << std::string(60, '=') << "\n\n";
        }

        return response;

    } catch (const std::exception& e) {
        last_error_ = std::string("処理中にエラーが発生: ") + e.what();
        if (debug_mode_) {
            std::cerr << "[エラー] " << last_error_ << "\n";
        }
        return "[エラー] " + last_error_;
    }
}

std::string EmotionalAgent::get_emotion_status() const {
    if (!emotion_engine_) {
        return "感情エンジンが初期化されていません。";
    }
    return emotion_engine_->describe_emotion();
}

std::string EmotionalAgent::get_conversation_history(int max_turns) const {
    if (!memory_controller_) {
        return "記憶コントローラーが初期化されていません。";
    }
    return memory_controller_->get_short_term_as_text(max_turns);
}

int EmotionalAgent::get_episode_count() const {
    if (!memory_controller_) {
        return 0;
    }
    return memory_controller_->get_long_term_size();
}

void EmotionalAgent::set_system_prompt(const std::string& system_prompt) {
    if (prompt_orchestrator_) {
        prompt_orchestrator_->set_system_prompt(system_prompt);
    }
}

void EmotionalAgent::add_system_log_section(
    const std::string& section_name,
    const std::string& content) {

    if (prompt_orchestrator_) {
        prompt_orchestrator_->add_system_log_section(section_name, content);
    }
}

void EmotionalAgent::clear_system_log_sections() {
    if (prompt_orchestrator_) {
        prompt_orchestrator_->clear_system_log_sections();
    }
}

void EmotionalAgent::set_constitution(const PersonalityConstitution& constitution) {
    if (emotion_engine_) {
        emotion_engine_->set_constitution(constitution);
    }
}

void EmotionalAgent::set_tool_executor(IToolExecutor* executor) {
    tool_executor_ = executor;
}

void EmotionalAgent::register_tool(const ToolSpec& spec, ToolRegistryExecutor::ToolHandler handler) {
    if (!owned_tool_registry_) {
        owned_tool_registry_ = std::make_unique<ToolRegistryExecutor>();
    }

    owned_tool_registry_->register_tool(spec, std::move(handler));

    // 外部実行器が未設定なら、内蔵レジストリを利用する
    if (!tool_executor_) {
        tool_executor_ = owned_tool_registry_.get();
    }
}

void EmotionalAgent::register_tool(
    const ToolSpec& spec,
    const ToolObjectSchema& schema,
    ToolRegistryExecutor::ToolHandler handler) {

    if (!owned_tool_registry_) {
        owned_tool_registry_ = std::make_unique<ToolRegistryExecutor>();
    }

    owned_tool_registry_->register_tool(spec, schema, std::move(handler));

    if (!tool_executor_) {
        tool_executor_ = owned_tool_registry_.get();
    }
}

void EmotionalAgent::set_max_tool_iterations(int max_iterations) {
    max_tool_iterations_ = std::max(0, max_iterations);
}

void EmotionalAgent::clear_history() {
    if (memory_controller_) {
        memory_controller_->clear_short_term();
    }
}

void EmotionalAgent::reset() {
    if (emotion_engine_) {
        emotion_engine_->reset();
    }
    if (memory_controller_) {
        memory_controller_->clear_short_term();
        memory_controller_->clear_long_term();
    }
}

bool EmotionalAgent::save_state_to_file(const std::string& file_path) const {
    if (!emotion_engine_ || !memory_controller_ || !prompt_orchestrator_) {
        return false;
    }

    AgentPersistentState state;
    state.constitution = emotion_engine_->get_constitution();
    state.emotion_state = emotion_engine_->get_current_state();

    state.short_term_limit = memory_controller_->get_short_term_limit();
    state.short_term_memory = memory_controller_->get_short_term_history();
    state.long_term_memory = memory_controller_->get_all_episodes();

    state.system_prompt = prompt_orchestrator_->get_system_prompt();
    state.tone_instruction = prompt_orchestrator_->get_tone_instruction();
    state.max_episodes = prompt_orchestrator_->get_max_episodes();
    state.short_term_turns = prompt_orchestrator_->get_short_term_turns();
    state.system_log_sections = prompt_orchestrator_->get_system_log_sections();

    state.debug_mode = debug_mode_;

    return AgentStatePersistence::save_to_file(file_path, state);
}

bool EmotionalAgent::load_state_from_file(const std::string& file_path) {
    if (!emotion_engine_ || !memory_controller_ || !prompt_orchestrator_) {
        return false;
    }

    AgentPersistentState state;
    if (!AgentStatePersistence::load_from_file(file_path, state)) {
        return false;
    }

    emotion_engine_->set_constitution(state.constitution);
    emotion_engine_->set_current_state(state.emotion_state);

    memory_controller_->set_short_term_limit(state.short_term_limit);
    memory_controller_->set_short_term_history(state.short_term_memory);
    memory_controller_->set_long_term_memory(state.long_term_memory);

    prompt_orchestrator_->set_system_prompt(state.system_prompt);
    prompt_orchestrator_->set_tone_instruction(state.tone_instruction);
    prompt_orchestrator_->set_max_episodes(state.max_episodes);
    prompt_orchestrator_->set_short_term_turns(state.short_term_turns);
    prompt_orchestrator_->set_system_log_sections(state.system_log_sections);

    set_debug_mode(state.debug_mode);

    return true;
}

void EmotionalAgent::print_debug_info() const {
    std::cout << "\n===== エージェント情報 =====\n";
    std::cout << "初期化状態: " << (initialized_ ? "完了" : "未完了") << "\n";
    std::cout << "モデルパス: " << model_path_ << "\n";
    std::cout << "\n=== 感情状態 ===\n";
    std::cout << get_emotion_status() << "\n";
    std::cout << "\n=== 短期メモリ ===\n";
    std::cout << "ターン数: " << memory_controller_->get_short_term_size() << "\n";
    std::cout << "\n=== 長期メモリ ===\n";
    std::cout << "エピソード数: " << get_episode_count() << "\n";
    std::cout << "==========================\n\n";
}

std::string EmotionalAgent::build_prompt_preview(
    const std::string& user_input,
    PromptOrchestrator::PromptPhase phase) {
    if (!initialized_) {
        return "[エラー] エージェントが初期化されていません。";
    }

    if (!prompt_orchestrator_ || !emotion_engine_ || !memory_controller_) {
        return "[エラー] プロンプト生成に必要なモジュールが初期化されていません。";
    }

    const auto original_sections = prompt_orchestrator_->get_system_log_sections();

    try {
        clear_system_log_sections();

        if (tool_use_enabled_ && tool_executor_) {
            const auto tools = tool_executor_->list_tools();
            if (!tools.empty()) {
                add_system_log_section("Tool Interface", ToolIOProtocol::build_tool_guide(tools));

                const auto has_number_guess_tool = std::any_of(
                    tools.begin(),
                    tools.end(),
                    [](const ToolSpec& spec) {
                        return spec.name == "number_guess_game";
                    }
                );

                if (has_number_guess_tool) {
                    add_system_log_section(
                        "Game Tool Policy",
                        "ユーザーが数当てゲームを希望した場合、推測や判定を文章で決め打ちせず"
                        " number_guess_game を使って進行してください。"
                        "開始時は action=start、進行中の判定は action=guess、"
                        "状態確認は action=status、終了/やり直しは action=reset を優先してください。"
                        "数値の正誤判定は必ずツール結果に従ってください。"
                    );
                }
            }
        }

        std::string preview_prompt = prompt_orchestrator_->build_final_prompt(
            user_input,
            *emotion_engine_,
            *memory_controller_,
            phase
        );

        prompt_orchestrator_->set_system_log_sections(original_sections);
        return preview_prompt;
    } catch (...) {
        prompt_orchestrator_->set_system_log_sections(original_sections);
        throw;
    }
}

void EmotionalAgent::consolidate_memories() {
    // 短期メモリが一定以上の長さになったら統合
    if (memory_controller_->get_short_term_size() >= 5) {
        std::string emotion_desc = emotion_engine_->describe_emotion();
        
        // 最近の会話からキーワードを抽出（簡易実装）
        std::vector<std::string> keywords;
        const auto& history = memory_controller_->get_short_term_history();
        for (const auto& turn : history) {
            auto turn_keywords = input_analyzer_->extract_keywords(turn.content);
            keywords.insert(keywords.end(), turn_keywords.begin(), turn_keywords.end());
        }

        // 重複を削除
        std::sort(keywords.begin(), keywords.end());
        keywords.erase(std::unique(keywords.begin(), keywords.end()), keywords.end());

        // 直近会話をLLMで要約（失敗時は空文字 → MemoryController側でフォールバック）
        std::string llm_summary = summarize_recent_conversation_with_llm();

        // 記憶を統合
        memory_controller_->consolidate_memory(emotion_desc, keywords, llm_summary);
    }
}

std::string EmotionalAgent::summarize_recent_conversation_with_llm() const {
    if (!llm_inference_ || !memory_controller_) {
        return "";
    }

    const auto& history = memory_controller_->get_short_term_history();
    if (history.empty()) {
        return "";
    }

    std::ostringstream history_text;
    for (const auto& turn : history) {
        history_text << turn.role << ": " << turn.content << "\n";
    }

    std::ostringstream prompt;
    prompt << "以下はユーザーとAIの会話履歴です。\n";
    prompt << "会話の要点を日本語で1〜2文、120文字以内で要約してください。\n";
    prompt << "出力は要約文のみ。前置きや箇条書き、見出し、説明は不要です。\n\n";
    prompt << "会話履歴:\n";
    prompt << history_text.str();
    prompt << "\n要約:";

    std::string summary = llm_inference_->infer(prompt.str());
    if (summary.empty()) {
        return "";
    }

    auto trim = [](std::string value) {
        const char* whitespace = " \t\r\n";
        const auto first = value.find_first_not_of(whitespace);
        if (first == std::string::npos) {
            return std::string();
        }
        const auto last = value.find_last_not_of(whitespace);
        value = value.substr(first, last - first + 1);
        return value;
    };

    summary = trim(summary);

    const std::vector<std::string> prefixes = {
        "要約:", "summary:", "Summary:", "応答:", "回答:"
    };

    for (const auto& prefix : prefixes) {
        if (summary.rfind(prefix, 0) == 0) {
            summary = trim(summary.substr(prefix.length()));
            break;
        }
    }

    auto newline_pos = summary.find('\n');
    if (newline_pos != std::string::npos) {
        summary = trim(summary.substr(0, newline_pos));
    }

    return summary;
}

void EmotionalAgent::set_debug_mode(bool enable) {
    debug_mode_ = enable;
    
    // 各モジュールにもデバッグモードを伝播
    if (input_analyzer_) {
        input_analyzer_->set_debug_mode(enable);
    }

    if (llm_inference_) {
        llm_inference_->set_debug_mode(enable);
    }
    
    if (enable) {
        std::cout << "[デバッグモード] 有効化されました\n";
    }
}
