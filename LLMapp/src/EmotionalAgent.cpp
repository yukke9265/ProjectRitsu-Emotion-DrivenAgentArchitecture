#include "EmotionalAgent.h"
#include "Config.h"
#include <algorithm> // 追加
#include <iostream>

EmotionalAgent::EmotionalAgent(
    const std::string& model_path,
    const PersonalityConstitution& constitution)
    : model_path_(model_path)
    , initialized_(false)
    , last_error_("") {
    
    // モジュールの初期化
    input_analyzer_ = std::make_unique<InputAnalyzer>();
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
    
    // LLMベースの分析モードを有効化（ハイブリッドモード）
    input_analyzer_->enable_llm_mode(true);

    initialized_ = true;
    return true;
}

std::string EmotionalAgent::process(const std::string& user_input) {
    if (!initialized_) {
        return "[エラー] エージェントが初期化されていません。";
    }

    try {
        // ===== 処理フロー =====

        // Step 1: 入力解析
        AnalyzedInput analyzed = input_analyzer_->analyze(user_input);

        // Step 2: 感情エンジンで評価・更新
        emotion_engine_->appraise_and_update(analyzed);
        emotion_engine_->apply_decay();  // 時間経過による減衰

        // Step 3: 短期メモリに追加
        memory_controller_->add_to_short_term("user", user_input);

        // Step 4: プロンプト生成
        std::string final_prompt = prompt_orchestrator_->build_final_prompt(
            user_input,
            *emotion_engine_,
            *memory_controller_
        );

        // デバッグ出力（オプション）
        // std::cout << "\n=== Generated Prompt ===\n" << final_prompt << "\n====================\n";

        // Step 5: LLMで応答生成
        std::string response = llm_inference_->infer(final_prompt);

        // Step 6: 応答を短期メモリに追加
        memory_controller_->add_to_short_term("assistant", response);

        // Step 7: 定期的に記憶を統合（簡易実装：毎回実行）
        consolidate_memories();

        return response;

    } catch (const std::exception& e) {
        last_error_ = std::string("処理中にエラーが発生: ") + e.what();
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

void EmotionalAgent::set_constitution(const PersonalityConstitution& constitution) {
    if (emotion_engine_) {
        emotion_engine_->set_constitution(constitution);
    }
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

        // 記憶を統合
        memory_controller_->consolidate_memory(emotion_desc, keywords);
    }
}
