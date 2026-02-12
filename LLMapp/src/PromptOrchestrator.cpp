#include "PromptOrchestrator.h"
#include <sstream>

PromptOrchestrator::PromptOrchestrator()
    : system_prompt_("あなたは親切で助けになるAIアシスタントです。")
    , tone_instruction_("")
    , max_episodes_(3)
    , short_term_turns_(5) {
}

PromptOrchestrator::~PromptOrchestrator() {
}

std::string PromptOrchestrator::build_final_prompt(
    const std::string& user_input,
    const EmotionEngine& emotion_engine,
    const MemoryController& memory_controller) {
    
    std::ostringstream prompt;

    // ===== 1. 人格憲法（システムプロンプト） =====
    prompt << "# あなたの人格憲法\n\n";
    prompt << system_prompt_ << "\n\n";

    const auto& constitution = emotion_engine.get_constitution();
    prompt << "## コア価値観\n";
    prompt << constitution.core_values << "\n\n";
    
    prompt << "## コミュニケーションスタイル\n";
    prompt << constitution.communication_style << "\n\n";

    // ===== 2. 現在の感情状態 =====
    prompt << "# 現在のあなたの感情状態\n\n";
    prompt << format_emotion_state(emotion_engine) << "\n\n";

    // ===== 3. トーン制御の指示 =====
    const auto& emotion_state = emotion_engine.get_current_state();
    std::string tone_control = generate_tone_control(emotion_state);
    if (!tone_control.empty()) {
        prompt << "# 応答トーンの制御\n\n";
        prompt << tone_control << "\n\n";
    }

    // ===== 4. 短期メモリ（直近の会話） =====
    std::string short_term = format_short_term_memory(memory_controller);
    if (!short_term.empty()) {
        prompt << "# 直近の会話履歴\n\n";
        prompt << short_term << "\n";
    }

    // ===== 5. 関連する長期記憶 =====
    // ユーザー入力からキーワードを抽出（簡易実装）
    std::vector<std::string> keywords;
    // TODO: より高度なキーワード抽出
    std::istringstream iss(user_input);
    std::string word;
    while (iss >> word && keywords.size() < 5) {
        if (word.length() >= 3) {
            keywords.push_back(word);
        }
    }

    std::string long_term = format_long_term_memory(memory_controller, keywords);
    if (!long_term.empty()) {
        prompt << "# 関連する過去の記憶\n\n";
        prompt << long_term << "\n";
    }

    // ===== 6. カスタムトーン指示 =====
    if (!tone_instruction_.empty()) {
        prompt << "# 追加の応答指示\n\n";
        prompt << tone_instruction_ << "\n\n";
    }

    // ===== 7. ユーザー入力 =====
    prompt << "# ユーザーの発言\n\n";
    prompt << user_input << "\n\n";

    // ===== 8. 最終指示 =====
    prompt << "---\n\n";
    prompt << "上記の情報を考慮して、現在の感情状態を反映した自然な応答を生成してください。\n";
    prompt << "感情を押し付けず、あなたの人格憲法に基づいた誠実な回答を心がけてください。\n";

    return prompt.str();
}

void PromptOrchestrator::set_system_prompt(const std::string& system_prompt) {
    system_prompt_ = system_prompt;
}

void PromptOrchestrator::set_tone_instruction(const std::string& tone_instruction) {
    tone_instruction_ = tone_instruction;
}

std::string PromptOrchestrator::format_emotion_state(const EmotionEngine& emotion_engine) {
    std::ostringstream oss;

    const auto& state = emotion_engine.get_current_state();

    // 感情の説明
    oss << emotion_engine.describe_emotion() << "\n\n";

    // 各感情の詳細値
    oss << "詳細:\n";
    for (const auto& pair : state.values) {
        if (pair.second > 0.1) {  // 0.1以上の感情のみ表示
            std::string emotion_name;
            switch (pair.first) {
                case BasicEmotion::JOY: emotion_name = "喜び"; break;
                case BasicEmotion::TRUST: emotion_name = "信頼"; break;
                case BasicEmotion::FEAR: emotion_name = "恐れ"; break;
                case BasicEmotion::SURPRISE: emotion_name = "驚き"; break;
                case BasicEmotion::SADNESS: emotion_name = "悲しみ"; break;
                case BasicEmotion::DISGUST: emotion_name = "嫌悪"; break;
                case BasicEmotion::ANGER: emotion_name = "怒り"; break;
                case BasicEmotion::ANTICIPATION: emotion_name = "期待"; break;
            }
            oss << "  - " << emotion_name << ": " << pair.second << "\n";
        }
    }

    return oss.str();
}

std::string PromptOrchestrator::format_short_term_memory(
    const MemoryController& memory_controller) {
    
    return memory_controller.get_short_term_as_text(short_term_turns_);
}

std::string PromptOrchestrator::format_long_term_memory(
    const MemoryController& memory_controller,
    const std::vector<std::string>& keywords) {
    
    if (keywords.empty()) {
        return "";
    }

    auto episodes = memory_controller.search_episodes(keywords, max_episodes_);
    
    if (episodes.empty()) {
        return "";
    }

    std::ostringstream oss;
    oss << "あなたが記憶している関連するエピソード:\n\n";

    for (size_t i = 0; i < episodes.size(); ++i) {
        const auto& episode = episodes[i];
        oss << (i + 1) << ". " << episode.summary;
        oss << " [" << episode.emotional_tag << ", 重要度: " << episode.importance << "]\n";
    }

    return oss.str();
}

std::string PromptOrchestrator::generate_tone_control(const EmotionState& emotion_state) {
    std::ostringstream oss;

    // 全体的な感情価に基づくトーン制御
    if (emotion_state.overall_valence > 0.5) {
        oss << "あなたは今、とてもポジティブな気分です。";
        oss << "回答は明るく、前向きなトーンで行ってください。\n";
    } else if (emotion_state.overall_valence > 0.2) {
        oss << "あなたは今、やや前向きな気分です。";
        oss << "自然で穏やかなトーンで回答してください。\n";
    } else if (emotion_state.overall_valence < -0.3) {
        oss << "あなたは今、やや落ち込んだ気分です。";
        oss << "控えめで慎重なトーンで回答してください。\n";
    } else {
        oss << "あなたは今、落ち着いた中立的な気分です。";
        oss << "バランスの取れたトーンで回答してください。\n";
    }

    // 覚醒度に基づく調整
    if (emotion_state.arousal > 0.7) {
        oss << "ただし、興奮しすぎないように注意してください。\n";
    } else if (emotion_state.arousal < 0.2) {
        oss << "少し活気を持たせた表現を心がけてください。\n";
    }

    return oss.str();
}
