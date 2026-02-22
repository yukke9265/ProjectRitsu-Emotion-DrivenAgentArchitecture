#include "PromptOrchestrator.h"
#include "Config.h"
#include <sstream>

namespace {

bool is_managed_constitution_heading(const std::string& line) {
    return line == "## コア価値観"
        || line == "## コミュニケーションスタイル"
        || line == "## 返答スタイル"
        || line == "## 返答の文章量について";
}

bool is_markdown_heading(const std::string& line) {
    return !line.empty() && line[0] == '#';
}

std::string strip_managed_constitution_sections(const std::string& system_prompt) {
    std::istringstream iss(system_prompt);
    std::ostringstream oss;
    std::string line;
    bool skipping_managed_section = false;
    bool wrote_any = false;

    while (std::getline(iss, line)) {
        if (is_managed_constitution_heading(line)) {
            skipping_managed_section = true;
            continue;
        }

        if (skipping_managed_section && is_markdown_heading(line)) {
            skipping_managed_section = false;
        }

        if (skipping_managed_section) {
            continue;
        }

        if (wrote_any) {
            oss << "\n";
        }
        oss << line;
        wrote_any = true;
    }

    return oss.str();
}

}

PromptOrchestrator::PromptOrchestrator()
    : system_prompt_(DEFAULT_SYSTEM_PROMPT)
    , tone_instruction_(
        "【感情調律指示】\n"
        "入力される感情パラメータに従い、言葉のトーンを微調整します。\n"
        "- 低Valence時: 論理的な不機嫌さ（素っ気なさ）を見せます。\n"
        "- 高Arousal時: 饒舌になるか、余裕を失って短気な反応を見せます。\n"
        "- 低Dominance時: 図星を突かれた際など、言葉を詰まらせる等の反応を挿入します。"
    )
    , response_style_instruction_(DEFAULT_RESPONSE_STYLE_GUIDELINES)
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
    const std::string base_system_prompt = strip_managed_constitution_sections(system_prompt_);

    // ===== 1. 人格憲法（システムプロンプト） =====
    prompt << "# あなたの人格憲法\n\n";
    prompt << base_system_prompt << "\n\n";

    const auto& constitution = emotion_engine.get_constitution();
    prompt << "## コア価値観\n";
    prompt << constitution.core_values << "\n\n";
    
    prompt << "## コミュニケーションスタイル\n";
    prompt << constitution.communication_style << "\n\n";

    if (!response_style_instruction_.empty()) {
        prompt << "## 返答スタイル\n";
        prompt << response_style_instruction_ << "\n\n";
    }

    // ===== 2. 現在の感情状態とトーン制御 =====
    prompt << "# 現在のあなたの感情状態と応答トーン\n\n";
    prompt << format_emotion_state(emotion_engine) << "\n\n";

    // 感情状態から生成されるトーン制御
    const auto& emotion_state = emotion_engine.get_current_state();
    std::string tone_control = generate_tone_control(emotion_state);
    if (!tone_control.empty()) {
        prompt << tone_control << "\n\n";
    }

    // カスタムトーン指示
    if (!tone_instruction_.empty()) {
        prompt << tone_instruction_ << "\n\n";
    }

    // ===== 3. 構造化システムログ（Output Contract除外） =====
    prompt << "# システムログ（構造化コンテキスト）\n\n";
    prompt << format_system_logs_excluding("Output Contract") << "\n\n";

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

    // ===== 6. 出力契約（独立セクション） =====
    const std::string output_contract = find_system_log_section("Output Contract");
    if (!output_contract.empty()) {
        prompt << "# 出力契約\n\n";
        prompt << output_contract << "\n\n";
    }

    // ===== 7. 応答指示 =====
    prompt << "---\n\n";
    prompt << "直近の会話履歴の最後の user 発言に対して応答してください。\n";
    prompt << "ユーザーに直接話しかける自然なセリフを生成してください。\n";
    prompt << "感情状態の数値や分析説明は出さず、自然な日本語の応答本文だけを返してください。\n";

    return prompt.str();
}

void PromptOrchestrator::set_system_prompt(const std::string& system_prompt) {
    system_prompt_ = system_prompt;
}

void PromptOrchestrator::set_tone_instruction(const std::string& tone_instruction) {
    tone_instruction_ = tone_instruction;
}

void PromptOrchestrator::set_response_style_instruction(
    const std::string& response_style_instruction) {

    response_style_instruction_ = response_style_instruction;
}

void PromptOrchestrator::add_system_log_section(
    const std::string& section_name,
    const std::string& content) {

    if (content.empty()) {
        return;
    }

    StructuredLogSection section;
    section.name = section_name.empty() ? "Untitled" : section_name;
    section.content = content;
    system_log_sections_.push_back(section);
}

void PromptOrchestrator::clear_system_log_sections() {
    system_log_sections_.clear();
}

void PromptOrchestrator::set_system_log_sections(
    const std::vector<StructuredLogSection>& sections) {

    system_log_sections_ = sections;
}

std::string PromptOrchestrator::format_system_logs() const {
    std::ostringstream oss;

    if (system_log_sections_.empty()) {
        oss << "- 現在、注入されているログはありません。";
        return oss.str();
    }

    for (size_t i = 0; i < system_log_sections_.size(); ++i) {
        const auto& section = system_log_sections_[i];
        const std::string title = section.name.empty() ? "Untitled" : section.name;

        oss << "## " << title << "\n";
        oss << "```text\n";
        oss << section.content << "\n";
        oss << "```\n";

        if (i + 1 < system_log_sections_.size()) {
            oss << "\n";
        }
    }

    return oss.str();
}

std::string PromptOrchestrator::format_system_logs_excluding(const std::string& excluded_section_name) const {
    std::ostringstream oss;
    bool has_any = false;

    for (size_t i = 0; i < system_log_sections_.size(); ++i) {
        const auto& section = system_log_sections_[i];
        const std::string title = section.name.empty() ? "Untitled" : section.name;

        if (title == excluded_section_name) {
            continue;
        }

        has_any = true;
        oss << "## " << title << "\n";
        oss << "```text\n";
        oss << section.content << "\n";
        oss << "```\n\n";
    }

    if (!has_any) {
        oss << "- 現在、注入されているログはありません。";
    }

    return oss.str();
}

std::string PromptOrchestrator::find_system_log_section(const std::string& section_name) const {
    for (const auto& section : system_log_sections_) {
        const std::string title = section.name.empty() ? "Untitled" : section.name;
        if (title == section_name) {
            return section.content;
        }
    }

    return "";
}

std::string PromptOrchestrator::format_emotion_state(const EmotionEngine& emotion_engine) {
    std::ostringstream oss;
    std::ostringstream detail_oss;
    bool has_detail = false;

    const auto& state = emotion_engine.get_current_state();

    // 感情の説明
    oss << emotion_engine.describe_emotion() << "\n\n";

    // 各感情の詳細値
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
            detail_oss << "  - " << emotion_name << ": " << pair.second << "\n";
            has_detail = true;
        }
    }

    if (has_detail) {
        oss << "詳細:\n";
        oss << detail_oss.str();
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
