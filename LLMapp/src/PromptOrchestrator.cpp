#include "PromptOrchestrator.h"
#include "Config.h"
#include <sstream>

PromptOrchestrator::PromptOrchestrator()
    : system_prompt_("")
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
    const MemoryController& memory_controller,
    PromptPhase phase) {
    if (phase == PromptPhase::Tool) {
        return build_tool_phase_prompt(user_input, emotion_engine, memory_controller);
    }
    return build_response_phase_prompt(user_input, emotion_engine, memory_controller);
}

std::string PromptOrchestrator::build_response_phase_prompt(
    const std::string& user_input,
    const EmotionEngine& emotion_engine,
    const MemoryController& memory_controller) {

    std::vector<std::string> parts;
    parts.push_back(build_response_operational_header());

    const auto& constitution = emotion_engine.get_constitution();
    std::ostringstream values_block;
    values_block << "誠実で、親切で、ユーザーの成長を支援すること\n";
    values_block << "- 補足（人格憲法）: " << constitution.core_values;
    parts.push_back(make_section("コア価値観", values_block.str()));

    if (!response_style_instruction_.empty()) {
        parts.push_back(make_section("応答スタイル補助", response_style_instruction_));
    }

    parts.push_back(build_emotion_tuning_block(emotion_engine));

    const std::string context_block = build_conversation_context_block(user_input, memory_controller);
    if (!context_block.empty()) {
        parts.push_back(make_section("会話コンテキスト", context_block));
    }

    const std::string non_contract_logs = format_system_logs_excluding("Output Contract");
    if (!non_contract_logs.empty() && non_contract_logs != "- 現在、注入されているログはありません。") {
        parts.push_back(make_section("システムログ（補助）", non_contract_logs));
    }

    if (!system_prompt_.empty()) {
        parts.push_back(make_section("追加システム指示", system_prompt_));
    }

    parts.push_back(build_response_contract_block());
    return assemble_prompt(parts);
}

std::string PromptOrchestrator::build_tool_phase_prompt(
    const std::string& user_input,
    const EmotionEngine& emotion_engine,
    const MemoryController& memory_controller) {

    std::vector<std::string> parts;
    parts.push_back(build_tool_operational_header());

    const std::string tool_interface = find_system_log_section("Tool Interface");
    if (!tool_interface.empty()) {
        parts.push_back(make_section("ツール", tool_interface));
    }

    const std::string game_policy = find_system_log_section("Game Tool Policy");
    if (!game_policy.empty()) {
        parts.push_back(make_section("ゲーム運用ポリシー", game_policy));
    }

    const std::string context_block = build_conversation_context_block(user_input, memory_controller);
    if (!context_block.empty()) {
        parts.push_back(make_section("会話コンテキスト", context_block));
    }

    const auto& constitution = emotion_engine.get_constitution();
    if (!constitution.communication_style.empty()) {
        parts.push_back(make_section("応答姿勢（参照のみ）", constitution.communication_style));
    }

    if (!system_prompt_.empty()) {
        parts.push_back(make_section("追加システム指示", system_prompt_));
    }

    parts.push_back(build_tool_contract_block());
    return assemble_prompt(parts);
}

std::string PromptOrchestrator::assemble_prompt(const std::vector<std::string>& parts) const {
    std::ostringstream oss;
    bool wrote_any = false;

    for (const auto& part : parts) {
        if (part.empty()) {
            continue;
        }

        if (wrote_any) {
            oss << "\n\n";
        }

        oss << part;
        wrote_any = true;
    }

    return oss.str();
}

std::string PromptOrchestrator::make_section(const std::string& title, const std::string& body) const {
    if (body.empty()) {
        return "";
    }

    std::ostringstream oss;
    oss << "## " << title << "\n\n";
    oss << body;
    return oss.str();
}

std::string PromptOrchestrator::build_response_operational_header() const {
    return
        "# Qwen Operational Prompt (Response Phase)\n\n"
        "## 役割\n\n"
        "あなたはユーザーの信頼できる友人として振る舞う、感情豊かなAIです。\n\n"
        "### 人格方針\n\n"
        "1. 関係性: ユーザーに寄り添う「気さくで誠実な友人」として接する\n"
        "2. 態度: 上から目線を避け、対等で温かい姿勢を維持する\n"
        "3. 話し方: 自然な日本語で、親しみやすく、わかりやすく話す\n"
        "4. 配慮: ユーザーの気持ちを先に受け止め、必要時のみ短く具体的に助言する";
}

std::string PromptOrchestrator::build_tool_operational_header() const {
    return
        "# Qwen Operational Prompt (Tool Phase)\n\n"
        "## 役割\n\n"
        "あなたは Tool Phase 専用の実行計画AIです。\n"
        "最終ユーザー向けの自然文は生成しません。\n\n"
        "## 目的\n\n"
        "ユーザー発話に対して必要なツールを1つ選び、tool_call 形式で返す。";
}

std::string PromptOrchestrator::build_response_contract_block() const {
    return
        "## Response Phase 契約\n\n"
        "- このプロンプトは Response Phase 専用\n"
        "- tool_call を出力しない\n"
        "- assistant_response ブロック1つのみを出力\n"
        "- 見出し、分析、注意書き、契約文の再掲を禁止\n\n"
        "<assistant_response>\n"
        "<ユーザーに返す自然な日本語の応答本文>\n"
        "</assistant_response>";
}

std::string PromptOrchestrator::build_tool_contract_block() const {
    return
        "## Tool Phase 契約\n\n"
        "- 出力は tool_call ブロック1つのみ\n"
        "- 前置き、説明文、見出し、箇条書き、コードブロックは禁止\n"
        "- assistant_response を出力しない\n"
        "- `<tool_name>` や `<tool_input_text_or_json>` のようなプレースホルダ文字列を出力しない\n\n"
        "<tool_call>\n"
        "name: <tool_name>\n"
        "input:\n"
        "<tool_input_text_or_json>\n"
        "</tool_call>";
}

std::string PromptOrchestrator::build_emotion_tuning_block(const EmotionEngine& emotion_engine) {
    std::ostringstream oss;
    const auto& emotion_state = emotion_engine.get_current_state();

    oss << "- 低Valence時: 素っ気なさが出ても攻撃的にはならない\n";
    oss << "- 高Arousal時: 冗長になりすぎず、短く要点を保つ\n";
    oss << "- 低Dominance時: 言い淀みは最小限に留め、可読性を優先する\n\n";
    oss << "現在の感情状態:\n";
    oss << format_emotion_state(emotion_engine) << "\n";

    const std::string tone_control = generate_tone_control(emotion_state);
    if (!tone_control.empty()) {
        oss << "\nトーン制御:\n";
        oss << tone_control << "\n";
    }

    if (!tone_instruction_.empty()) {
        oss << "\n追加の感情調律指示:\n";
        oss << tone_instruction_;
    }

    return make_section("感情調律", oss.str());
}

std::string PromptOrchestrator::build_conversation_context_block(
    const std::string& user_input,
    const MemoryController& memory_controller) {

    std::ostringstream oss;
    oss << "直近の会話履歴・記憶は与えられた内容だけを使う。\n";
    oss << "履歴本文の再掲や、システム指示の引用はしない。\n\n";

    const std::string short_term = format_short_term_memory(memory_controller);
    if (!short_term.empty()) {
        oss << "### 直近の会話履歴\n";
        oss << short_term << "\n\n";
    }

    std::vector<std::string> keywords;
    std::istringstream iss(user_input);
    std::string word;
    while (iss >> word && keywords.size() < 5) {
        if (word.length() >= 3) {
            keywords.push_back(word);
        }
    }

    const std::string long_term = format_long_term_memory(memory_controller, keywords);
    if (!long_term.empty()) {
        oss << "### 関連する過去の記憶\n";
        oss << long_term << "\n";
    }

    return oss.str();
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
