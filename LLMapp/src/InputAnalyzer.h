#pragma once

#include <string>
#include <vector>

// 前方宣言
class LLMInference;

/**
 * @brief ユーザー発言の構造化データ
 */
struct AnalyzedInput {
    std::string raw_text;              // 元の発言
    std::string topic;                 // トピック（何について話しているか）
    std::string intent;                // 意図（質問/賞賛/批判/雑談など）
    std::string evaluation_to_ai;      // AIへの評価（positive/neutral/negative）
    std::vector<std::string> keywords; // 抽出されたキーワード
    double sentiment_score;            // 感情スコア（-1.0 ~ 1.0）
};

/**
 * @brief 入力解析モジュール (Input Analyzer)
 * 
 * ユーザーの発言を「感情生成」のために構造化するフロントエージェント。
 * 発言から「トピック」「意図」「AIへの評価」を抽出し、
 * 後続の感情エンジンが判断しやすい形式に変換します。
 */
class InputAnalyzer {
public:
    InputAnalyzer();
    ~InputAnalyzer();

    /**
     * @brief LLMインスタンスを設定（EmotionalAgentから共有）
     * @param llm LLMInferenceへのポインタ
     */
    void set_llm_inference(LLMInference* llm);

    /**
     * @brief LLMベースの分析モードを切り替え
     * @param enable trueでLLMモード有効、falseでキーワードベースのみ
     */
    void enable_llm_mode(bool enable);

    /**
     * @brief LLMモードの状態を取得
     * @return LLMモードが有効ならtrue
     */
    bool is_llm_mode_enabled() const { return use_llm_; }

    /**
     * @brief ユーザー発言を構造化データに変換
     * @param user_input ユーザーの生の発言
     * @return 構造化された入力データ
     */
    AnalyzedInput analyze(const std::string& user_input);

    /**
     * @brief 感情スコアを計算（-1.0 = 非常にネガティブ, +1.0 = 非常にポジティブ）
     * @param text 評価する文章
     * @return 感情スコア
     */
    double calculate_sentiment(const std::string& text);

    /**
     * @brief 発言の意図を分類
     * @param text 分析する文章
     * @return 意図（"question", "praise", "criticism", "casual", "unknown"）
     */
    std::string classify_intent(const std::string& text);

    /**
     * @brief AIへの評価を判定
     * @param text 分析する文章
     * @return 評価（"positive", "neutral", "negative"）
     */
    std::string evaluate_ai_attitude(const std::string& text);

    /**
     * @brief キーワード抽出（簡易実装）
     * @param text 分析する文章
     * @return 抽出されたキーワードリスト
     */
    std::vector<std::string> extract_keywords(const std::string& text);

private:
    // ポジティブ/ネガティブなキーワード辞書（拡張可能）
    std::vector<std::string> positive_keywords_;
    std::vector<std::string> negative_keywords_;
    std::vector<std::string> praise_keywords_;
    std::vector<std::string> criticism_keywords_;

    // LLM関連
    LLMInference* llm_inference_;  // LLMインスタンス（所有権なし）
    bool use_llm_;                  // LLMモードのフラグ

    void initialize_dictionaries();
    std::string extract_topic(const std::string& text);

    // LLMベースの分析メソッド
    /**
     * @brief LLMを使用した入力分析
     * @param user_input ユーザーの発言
     * @return 構造化された入力データ
     */
    AnalyzedInput analyze_with_llm(const std::string& user_input);

    /**
     * @brief キーワードベースの分析（従来方式）
     * @param user_input ユーザーの発言
     * @return 構造化された入力データ
     */
    AnalyzedInput analyze_with_keywords(const std::string& user_input);

    /**
     * @brief LLM分析用のプロンプトを生成
     * @param user_input ユーザーの発言
     * @return 分析用プロンプト
     */
    std::string create_analysis_prompt(const std::string& user_input);

    /**
     * @brief LLMの応答をパースして構造化データに変換
     * @param llm_output LLMからの応答
     * @param raw_text 元のユーザー発言
     * @return 構造化された入力データ
     */
    AnalyzedInput parse_llm_response(const std::string& llm_output, 
                                     const std::string& raw_text);
};
