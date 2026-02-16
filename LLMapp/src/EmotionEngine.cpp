#include "EmotionEngine.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <sstream>

EmotionEngine::EmotionEngine() 
    : constitution_()
    , current_state_() {
}

EmotionEngine::EmotionEngine(const PersonalityConstitution& constitution)
    : constitution_(constitution)
    , current_state_() {
    // ベースラインを設定
    current_state_.overall_valence = constitution_.baseline_valence;
}

EmotionEngine::~EmotionEngine() {
}

void EmotionEngine::appraise_and_update(const AnalyzedInput& analyzed_input) {
    // 1. 入力を評価して感情変化量を計算
    auto deltas = calculate_emotion_deltas(analyzed_input);

    // 2. 感情値を更新
    update_emotion_values(deltas);

    // 3. 全体的な指標を更新
    update_overall_metrics();

    // 4. 更新時刻を記録
    current_state_.last_update = std::chrono::system_clock::now();
}

std::map<BasicEmotion, double> EmotionEngine::calculate_emotion_deltas(
    const AnalyzedInput& analyzed_input) {
    
    std::map<BasicEmotion, double> deltas;

    // 初期化
    for (auto& pair : current_state_.values) {
        deltas[pair.first] = 0.0;
    }

    const double context_multiplier = get_context_multiplier(analyzed_input);

    auto add_delta = [&](BasicEmotion emotion, double delta) {
        deltas[emotion] += delta * context_multiplier;
    };

    // 意図に基づく感情変化
    switch (analyzed_input.intent) {
        case Intent::PRAISE:
            add_delta(BasicEmotion::JOY, EMOTION_DELTA_PRAISE_JOY * constitution_.sensitivity_to_praise);
            add_delta(BasicEmotion::TRUST, EMOTION_DELTA_PRAISE_TRUST * constitution_.sensitivity_to_praise);
            break;
        case Intent::CRITICISM:
            add_delta(BasicEmotion::SADNESS, EMOTION_DELTA_CRITICISM_SADNESS * constitution_.sensitivity_to_criticism);
            add_delta(BasicEmotion::ANGER, EMOTION_DELTA_CRITICISM_ANGER * constitution_.sensitivity_to_criticism);
            add_delta(BasicEmotion::JOY, EMOTION_DELTA_CRITICISM_JOY * constitution_.sensitivity_to_criticism);
            break;
        case Intent::QUESTION:
            add_delta(BasicEmotion::ANTICIPATION, EMOTION_DELTA_QUESTION_ANTICIPATION);
            break;
        case Intent::GREETING:
            add_delta(BasicEmotion::JOY, EMOTION_DELTA_GREETING_JOY);
            break;
        case Intent::CASUAL:
        case Intent::UNKNOWN:
            break;
    }

    // AIへの評価に基づく感情変化
    switch (analyzed_input.evaluation_to_ai) {
        case EvaluationToAI::POSITIVE:
            add_delta(BasicEmotion::JOY, EMOTION_DELTA_AI_POSITIVE_JOY * constitution_.sensitivity_to_praise);
            add_delta(BasicEmotion::TRUST, EMOTION_DELTA_AI_POSITIVE_TRUST * constitution_.sensitivity_to_praise);
            break;
        case EvaluationToAI::NEGATIVE:
            add_delta(BasicEmotion::SADNESS, EMOTION_DELTA_AI_NEGATIVE_SADNESS * constitution_.sensitivity_to_criticism);
            add_delta(BasicEmotion::DISGUST, EMOTION_DELTA_AI_NEGATIVE_DISGUST * constitution_.sensitivity_to_criticism);
            add_delta(BasicEmotion::TRUST, EMOTION_DELTA_AI_NEGATIVE_TRUST * constitution_.sensitivity_to_criticism);
            break;
        case EvaluationToAI::NEUTRAL:
        case EvaluationToAI::UNKNOWN:
            break;
    }

    // 感情スコアに基づく全体的な調整
    double sentiment = analyzed_input.sentiment_score;
    if (sentiment > EMOTION_SENTIMENT_POSITIVE_THRESHOLD) {
        add_delta(BasicEmotion::JOY, sentiment * EMOTION_SENTIMENT_TO_JOY_SCALE);
    } else if (sentiment < EMOTION_SENTIMENT_NEGATIVE_THRESHOLD) {
        add_delta(BasicEmotion::SADNESS, std::abs(sentiment) * EMOTION_SENTIMENT_TO_SADNESS_SCALE);
        add_delta(BasicEmotion::ANGER, std::abs(sentiment) * EMOTION_SENTIMENT_TO_ANGER_SCALE);
    }

    // 一次変化に対する二次的な感情連鎖を追加
    apply_emotion_chain_effects(deltas);

    return deltas;
}

void EmotionEngine::update_emotion_values(const std::map<BasicEmotion, double>& deltas) {
    for (const auto& delta_pair : deltas) {
        BasicEmotion emotion = delta_pair.first;
        double delta = delta_pair.second;

        // 現在の値に変化量を加算
        current_state_.values[emotion] += delta;

        // 0.0 ~ 1.0 の範囲にクランプ
        current_state_.values[emotion] = std::max(0.0, std::min(1.0, current_state_.values[emotion]));
    }
}

void EmotionEngine::update_overall_metrics() {
    // 全体的な感情価を計算（ポジティブ - ネガティブ）
    double positive = current_state_.values[BasicEmotion::JOY] +
                     current_state_.values[BasicEmotion::TRUST] +
                     current_state_.values[BasicEmotion::ANTICIPATION];

    double negative = current_state_.values[BasicEmotion::SADNESS] +
                     current_state_.values[BasicEmotion::ANGER] +
                     current_state_.values[BasicEmotion::DISGUST] +
                     current_state_.values[BasicEmotion::FEAR];

    current_state_.overall_valence = (positive - negative) / EMOTION_VALENCE_NORMALIZATION_DIVISOR; // 正規化
    current_state_.overall_valence = std::max(-1.0, std::min(1.0, current_state_.overall_valence));

    // 覚醒度を計算（感情の総量）
    double total_emotion = 0.0;
    for (const auto& pair : current_state_.values) {
        total_emotion += pair.second;
    }
    current_state_.arousal = std::min(1.0, total_emotion / EMOTION_AROUSAL_NORMALIZATION_DIVISOR); // 正規化
}

void EmotionEngine::apply_decay() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - current_state_.last_update);

    if (duration.count() <= 0) {
        return;
    }

    // 各感情値をベースラインに向けて減衰
    for (auto& pair : current_state_.values) {
        const double decay_rate = get_decay_rate_for_emotion(pair.first);
        const double decay_factor = std::pow(1.0 - decay_rate, duration.count());
        pair.second *= decay_factor;
    }

    // 更新時刻を記録
    current_state_.last_update = now;

    // 全体的な指標を再計算
    update_overall_metrics();
}

std::string EmotionEngine::describe_emotion() const {
    std::ostringstream oss;

    // 最も強い感情を取得
    auto dominant = get_dominant_emotion();

    oss << "感情状態: " << emotion_to_string(dominant.first);
    
    if (dominant.second < EMOTION_DOMINANT_WEAK_THRESHOLD) {
        oss << "（弱い）";
    } else if (dominant.second < EMOTION_DOMINANT_STRONG_THRESHOLD) {
        oss << "（中程度）";
    } else {
        oss << "（強い）";
    }

    oss << " | 感情価: ";
    if (current_state_.overall_valence > EMOTION_VALENCE_POSITIVE_THRESHOLD) {
        oss << "ポジティブ";
    } else if (current_state_.overall_valence < EMOTION_VALENCE_NEGATIVE_THRESHOLD) {
        oss << "ネガティブ";
    } else {
        oss << "ニュートラル";
    }

    oss << " (" << current_state_.overall_valence << ")";
    oss << " | 覚醒度: " << current_state_.arousal;

    return oss.str();
}

void EmotionEngine::set_constitution(const PersonalityConstitution& constitution) {
    constitution_ = constitution;
    // ベースラインを再設定
    current_state_.overall_valence = constitution_.baseline_valence;
}

void EmotionEngine::set_current_state(const EmotionState& state) {
    current_state_ = state;

    // 欠落キーの補完
    current_state_.values[BasicEmotion::JOY] = std::max(0.0, std::min(1.0, current_state_.values[BasicEmotion::JOY]));
    current_state_.values[BasicEmotion::TRUST] = std::max(0.0, std::min(1.0, current_state_.values[BasicEmotion::TRUST]));
    current_state_.values[BasicEmotion::FEAR] = std::max(0.0, std::min(1.0, current_state_.values[BasicEmotion::FEAR]));
    current_state_.values[BasicEmotion::SURPRISE] = std::max(0.0, std::min(1.0, current_state_.values[BasicEmotion::SURPRISE]));
    current_state_.values[BasicEmotion::SADNESS] = std::max(0.0, std::min(1.0, current_state_.values[BasicEmotion::SADNESS]));
    current_state_.values[BasicEmotion::DISGUST] = std::max(0.0, std::min(1.0, current_state_.values[BasicEmotion::DISGUST]));
    current_state_.values[BasicEmotion::ANGER] = std::max(0.0, std::min(1.0, current_state_.values[BasicEmotion::ANGER]));
    current_state_.values[BasicEmotion::ANTICIPATION] = std::max(0.0, std::min(1.0, current_state_.values[BasicEmotion::ANTICIPATION]));

    current_state_.overall_valence = std::max(-1.0, std::min(1.0, current_state_.overall_valence));
    current_state_.arousal = std::max(0.0, std::min(1.0, current_state_.arousal));
}

void EmotionEngine::reset() {
    current_state_ = EmotionState();
    current_state_.overall_valence = constitution_.baseline_valence;
}

std::pair<BasicEmotion, double> EmotionEngine::get_dominant_emotion() const {
    BasicEmotion dominant = BasicEmotion::JOY;
    double max_value = 0.0;

    for (const auto& pair : current_state_.values) {
        if (pair.second > max_value) {
            max_value = pair.second;
            dominant = pair.first;
        }
    }

    return {dominant, max_value};
}

std::string EmotionEngine::emotion_to_string(BasicEmotion emotion) const {
    switch (emotion) {
        case BasicEmotion::JOY: return "喜び";
        case BasicEmotion::TRUST: return "信頼";
        case BasicEmotion::FEAR: return "恐れ";
        case BasicEmotion::SURPRISE: return "驚き";
        case BasicEmotion::SADNESS: return "悲しみ";
        case BasicEmotion::DISGUST: return "嫌悪";
        case BasicEmotion::ANGER: return "怒り";
        case BasicEmotion::ANTICIPATION: return "期待";
        default: return "不明";
    }
}

double EmotionEngine::get_context_multiplier(const AnalyzedInput& analyzed_input) const {
    auto has_keyword = [&](const std::initializer_list<const char*>& words) {
        for (const auto& keyword : analyzed_input.keywords) {
            for (const auto* word : words) {
                if (keyword == word) {
                    return true;
                }
            }
        }
        return false;
    };

    double multiplier = 1.0;

    switch (analyzed_input.intent) {
        case Intent::CRITICISM:
            if (has_keyword({"改善", "提案", "具体", "建設的", "constructive", "suggestion"})) {
                multiplier *= EMOTION_CONTEXT_CONSTRUCTIVE_MULTIPLIER;
            } else if (has_keyword({"最悪", "無能", "嫌い", "ひどい", "使えない"})) {
                multiplier *= EMOTION_CONTEXT_HARSH_MULTIPLIER;
            }
            break;
        case Intent::PRAISE:
            if (has_keyword({"ありがとう", "助かる", "great", "excellent", "最高"})) {
                multiplier *= EMOTION_CONTEXT_STRONG_PRAISE_MULTIPLIER;
            }
            break;
        case Intent::QUESTION:
        case Intent::GREETING:
        case Intent::CASUAL:
        case Intent::UNKNOWN:
            break;
    }

    return multiplier;
}

void EmotionEngine::apply_emotion_chain_effects(std::map<BasicEmotion, double>& deltas) const {
    const double anger_delta = std::max(0.0, deltas[BasicEmotion::ANGER]);
    const double sadness_delta = std::max(0.0, deltas[BasicEmotion::SADNESS]);
    const double surprise_delta = std::max(0.0, deltas[BasicEmotion::SURPRISE]);
    const double joy_delta = std::max(0.0, deltas[BasicEmotion::JOY]);

    deltas[BasicEmotion::DISGUST] += anger_delta * EMOTION_CHAIN_ANGER_TO_DISGUST;
    deltas[BasicEmotion::ANGER] += sadness_delta * EMOTION_CHAIN_SADNESS_TO_ANGER;
    deltas[BasicEmotion::ANTICIPATION] += surprise_delta * EMOTION_CHAIN_SURPRISE_TO_ANTICIPATION;
    deltas[BasicEmotion::FEAR] += surprise_delta * EMOTION_CHAIN_SURPRISE_TO_FEAR;
    deltas[BasicEmotion::TRUST] += joy_delta * EMOTION_CHAIN_JOY_TO_TRUST;
}

double EmotionEngine::get_decay_rate_for_emotion(BasicEmotion emotion) const {
    double decay_rate = constitution_.decay_rate;

    switch (emotion) {
        case BasicEmotion::ANGER:
        case BasicEmotion::DISGUST:
        case BasicEmotion::SADNESS:
            decay_rate *= EMOTION_DECAY_NEGATIVE_MULTIPLIER;
            break;
        case BasicEmotion::SURPRISE:
            decay_rate *= EMOTION_DECAY_SURPRISE_MULTIPLIER;
            break;
        case BasicEmotion::FEAR:
        case BasicEmotion::ANTICIPATION:
            decay_rate *= EMOTION_DECAY_FEAR_ANTICIPATION_MULTIPLIER;
            break;
        case BasicEmotion::JOY:
        case BasicEmotion::TRUST:
            decay_rate *= EMOTION_DECAY_POSITIVE_MULTIPLIER;
            break;
    }

    return std::max(0.0, std::min(EMOTION_DECAY_RATE_MAX, decay_rate));
}
