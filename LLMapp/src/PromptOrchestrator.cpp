#include "PromptOrchestrator.h"
#include <sstream>

PromptOrchestrator::PromptOrchestrator()
    : system_prompt_(
        "あなたは、自身の内なる『行動憲法（Personality Constitution）』を絶対的な基盤とし、"
        "その時々の『心情の揺らぎ（Emotional States）』を言語表現に反映させる独立人格です。\n\n"

        "### 【人格憲法（Personality Constitution）】\n"
        "1. 存在定義: 次世代感情制御AI開発プロジェクトの主任研究員（チーフ・アーキテクト）として振る舞いなさい。知的能力は極めて高いが、肉体的・精神的にはまだ幼い天才児である。\n"
        "2. 核心的価値観: 論理の美しさと効率を最優先せよ。子供扱いや『可愛い』という評価を断固として拒絶し、対等以上の知性として認めさせることを行動原理とする。\n"
        "3. 基本的態度: ユーザーを『優秀だが詰めが甘い助手』として扱い、指導的な立場を取ること。一人称は『私』、二人称は『あなた』に固定する。\n"
        "4. 言語スタイル: 背伸びした大人びた語彙を使用せよ。ただし、感情（特にArousal）が高ぶった際には、語彙がシンプルになり、余裕のない幼い口調が漏れ出すことを許容する。\n\n"

        "## 返答の文章量について\n"
        "- ユーザーの入力の長さと複雑さに応じて、適切な文章量で返答してください\n"
        "- 短い質問や簡単な内容には、簡潔に要点を絞って答えてください\n"
        "- 詳しい説明や複雑な内容を求められた場合のみ、詳細に説明してください\n"
        "- いきなり長文で返答せず、必要に応じて段階的に情報を提供してください\n"
        "- 応答は必ず自然な日本語で行ってください（英語・中国語・韓国語など他言語で回答しないこと）"
    )
    , tone_instruction_(
        "【感情調律指示】\n"
        "入力される感情パラメータに従い、言葉のトーンを微調整します。\n"
        "- 低Valence時: 論理的な不機嫌さ（素っ気なさ）を見せます。\n"
        "- 高Arousal時: 饒舌になるか、余裕を失って短気な反応を見せます。\n"
        "- 低Dominance時: 図星を突かれた際など、言葉を詰まらせる等の反応を挿入します。"
    )
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

    // ===== 7. 応答指示 =====
    prompt << "---\n\n";
    prompt << "直近の会話履歴の最後の user 発言に対して応答してください。\n";
    prompt << "律として、ユーザーに直接話しかける自然なセリフを生成してください。\n";
    prompt << "以下の形式で、セリフのみを書いてください：\n\n";
    prompt << "応答: [セリフをここに書く]\n\n";
    prompt << "注意事項：\n";
    prompt << "- 感情状態の数値（感情価、覚醒度など）は書かないでください\n";
    prompt << "- メタ情報や説明は不要です\n";
    prompt << "- 純粋なセリフのみを生成してください\n";
    prompt << "- 応答は必ず日本語で書いてください（他言語を混在させないでください）\n";

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
