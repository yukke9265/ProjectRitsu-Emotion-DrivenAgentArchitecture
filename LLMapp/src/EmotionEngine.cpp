#include "EmotionEngine.h"
#include <algorithm>
#include <cmath>
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

    // 意図に基づく感情変化
    if (analyzed_input.intent == "praise") {
        deltas[BasicEmotion::JOY] += 0.3 * constitution_.sensitivity_to_praise;
        deltas[BasicEmotion::TRUST] += 0.2 * constitution_.sensitivity_to_praise;
    } 
    else if (analyzed_input.intent == "criticism") {
        deltas[BasicEmotion::SADNESS] += 0.2 * constitution_.sensitivity_to_criticism;
        deltas[BasicEmotion::ANGER] += 0.15 * constitution_.sensitivity_to_criticism;
        deltas[BasicEmotion::JOY] -= 0.1 * constitution_.sensitivity_to_criticism;
    }
    else if (analyzed_input.intent == "question") {
        deltas[BasicEmotion::ANTICIPATION] += 0.15;
    }
    else if (analyzed_input.intent == "greeting") {
        deltas[BasicEmotion::JOY] += 0.1;
    }

    // AIへの評価に基づく感情変化
    if (analyzed_input.evaluation_to_ai == "positive") {
        deltas[BasicEmotion::JOY] += 0.2 * constitution_.sensitivity_to_praise;
        deltas[BasicEmotion::TRUST] += 0.25 * constitution_.sensitivity_to_praise;
    }
    else if (analyzed_input.evaluation_to_ai == "negative") {
        deltas[BasicEmotion::SADNESS] += 0.15 * constitution_.sensitivity_to_criticism;
        deltas[BasicEmotion::DISGUST] += 0.1 * constitution_.sensitivity_to_criticism;
        deltas[BasicEmotion::TRUST] -= 0.1 * constitution_.sensitivity_to_criticism;
    }

    // 感情スコアに基づく全体的な調整
    double sentiment = analyzed_input.sentiment_score;
    if (sentiment > 0.3) {
        deltas[BasicEmotion::JOY] += sentiment * 0.2;
    } else if (sentiment < -0.3) {
        deltas[BasicEmotion::SADNESS] += std::abs(sentiment) * 0.15;
        deltas[BasicEmotion::ANGER] += std::abs(sentiment) * 0.1;
    }

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

    current_state_.overall_valence = (positive - negative) / 7.0; // 正規化
    current_state_.overall_valence = std::max(-1.0, std::min(1.0, current_state_.overall_valence));

    // 覚醒度を計算（感情の総量）
    double total_emotion = 0.0;
    for (const auto& pair : current_state_.values) {
        total_emotion += pair.second;
    }
    current_state_.arousal = std::min(1.0, total_emotion / 4.0); // 正規化
}

void EmotionEngine::apply_decay() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - current_state_.last_update);

    // 時間経過に基づく減衰（1秒ごとに減衰率を適用）
    double decay_factor = std::pow(1.0 - constitution_.decay_rate, duration.count());

    // 各感情値をベースラインに向けて減衰
    for (auto& pair : current_state_.values) {
        pair.second *= decay_factor;
    }

    // 全体的な感情価もベースラインに向けて減衰
    current_state_.overall_valence = 
        constitution_.baseline_valence + 
        (current_state_.overall_valence - constitution_.baseline_valence) * decay_factor;

    // 覚醒度も減衰
    current_state_.arousal *= decay_factor;

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
    
    if (dominant.second < 0.3) {
        oss << "（弱い）";
    } else if (dominant.second < 0.6) {
        oss << "（中程度）";
    } else {
        oss << "（強い）";
    }

    oss << " | 感情価: ";
    if (current_state_.overall_valence > 0.3) {
        oss << "ポジティブ";
    } else if (current_state_.overall_valence < -0.3) {
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
