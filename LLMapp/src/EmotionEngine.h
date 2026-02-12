#pragma once

#include "InputAnalyzer.h"
#include <string>
#include <map>
#include <chrono>

/**
 * @brief 基本感情の種類
 */
enum class BasicEmotion {
    JOY,        // 喜び
    TRUST,      // 信頼
    FEAR,       // 恐れ
    SURPRISE,   // 驚き
    SADNESS,    // 悲しみ
    DISGUST,    // 嫌悪
    ANGER,      // 怒り
    ANTICIPATION // 期待
};

/**
 * @brief 感情状態（複数の感情値を保持）
 */
struct EmotionState {
    std::map<BasicEmotion, double> values;  // 各感情の強度（0.0 ~ 1.0）
    double overall_valence;                 // 全体的な感情価（-1.0 ~ 1.0）
    double arousal;                         // 覚醒度（0.0 ~ 1.0）
    std::chrono::system_clock::time_point last_update; // 最終更新時刻

    EmotionState() : overall_valence(0.0), arousal(0.0) {
        // 全ての感情を0で初期化
        values[BasicEmotion::JOY] = 0.0;
        values[BasicEmotion::TRUST] = 0.0;
        values[BasicEmotion::FEAR] = 0.0;
        values[BasicEmotion::SURPRISE] = 0.0;
        values[BasicEmotion::SADNESS] = 0.0;
        values[BasicEmotion::DISGUST] = 0.0;
        values[BasicEmotion::ANGER] = 0.0;
        values[BasicEmotion::ANTICIPATION] = 0.0;
        last_update = std::chrono::system_clock::now();
    }
};

/**
 * @brief 人格憲法（不変の価値観）
 */
struct PersonalityConstitution {
    std::string core_values;        // コア価値観
    std::string communication_style; // コミュニケーションスタイル
    double sensitivity_to_praise;    // 賞賛への感受性（0.0 ~ 1.0）
    double sensitivity_to_criticism; // 批判への感受性（0.0 ~ 1.0）
    double decay_rate;               // 感情の減衰率（0.0 ~ 1.0）
    double baseline_valence;         // 基準感情価（-1.0 ~ 1.0）

    PersonalityConstitution()
        : core_values("助けになり、親切で、誠実であること")
        , communication_style("丁寧で共感的")
        , sensitivity_to_praise(0.7)
        , sensitivity_to_criticism(0.5)
        , decay_rate(0.1)
        , baseline_valence(0.3)  // やや前向きなベースライン
    {}
};

/**
 * @brief 感情エンジン (Emotion Engine)
 * 
 * 感情モジュールの実体であり、状態を管理する心臓部。
 * Appraisal（評価）と State Update（状態更新）を実行します。
 */
class EmotionEngine {
public:
    EmotionEngine();
    explicit EmotionEngine(const PersonalityConstitution& constitution);
    ~EmotionEngine();

    /**
     * @brief 入力に基づいて感情を評価・更新
     * @param analyzed_input 構造化された入力データ
     */
    void appraise_and_update(const AnalyzedInput& analyzed_input);

    /**
     * @brief 時間経過による感情の減衰を適用
     */
    void apply_decay();

    /**
     * @brief 現在の感情状態を取得
     * @return 現在の感情状態
     */
    const EmotionState& get_current_state() const { return current_state_; }

    /**
     * @brief 人格憲法を取得
     * @return 人格憲法
     */
    const PersonalityConstitution& get_constitution() const { return constitution_; }

    /**
     * @brief 感情状態をテキスト表現に変換
     * @return 感情の説明文
     */
    std::string describe_emotion() const;

    /**
     * @brief 人格憲法を設定
     * @param constitution 新しい人格憲法
     */
    void set_constitution(const PersonalityConstitution& constitution);

    /**
     * @brief 感情状態をリセット
     */
    void reset();

private:
    PersonalityConstitution constitution_; // 人格憲法（不変）
    EmotionState current_state_;           // 現在の感情状態（可変）

    /**
     * @brief 入力を評価して感情変化量を計算
     * @param analyzed_input 構造化された入力データ
     * @return 感情変化量のマップ
     */
    std::map<BasicEmotion, double> calculate_emotion_deltas(
        const AnalyzedInput& analyzed_input);

    /**
     * @brief 感情値を更新（0.0 ~ 1.0の範囲にクランプ）
     * @param deltas 感情変化量
     */
    void update_emotion_values(const std::map<BasicEmotion, double>& deltas);

    /**
     * @brief 全体的な感情価と覚醒度を計算
     */
    void update_overall_metrics();

    /**
     * @brief 最も強い感情を取得
     * @return 最も強い感情とその値のペア
     */
    std::pair<BasicEmotion, double> get_dominant_emotion() const;

    /**
     * @brief 感情名を文字列に変換
     * @param emotion 感情の種類
     * @return 感情名の文字列
     */
    std::string emotion_to_string(BasicEmotion emotion) const;
};
