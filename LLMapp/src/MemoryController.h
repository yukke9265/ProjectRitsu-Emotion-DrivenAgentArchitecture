#pragma once

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <chrono>

/**
 * @brief 会話ターンの記録
 */
struct ConversationTurn {
    std::string role;     // "user" または "assistant"
    std::string content;  // 発言内容
    std::chrono::system_clock::time_point timestamp; // 発言時刻

    ConversationTurn(const std::string& r, const std::string& c)
        : role(r), content(c), timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief 長期記憶のエピソード
 */
struct Episode {
    std::string summary;        // エピソードの要約
    std::string emotional_tag;  // 感情タグ（"positive", "negative", "neutral"）
    double importance;          // 重要度（0.0 ~ 1.0）
    std::chrono::system_clock::time_point timestamp; // 記録時刻
    std::vector<std::string> keywords; // 検索用キーワード

    Episode()
        : importance(0.0)
        , timestamp(std::chrono::system_clock::now())
    {}

    Episode(const std::string& sum, const std::string& tag, double imp)
        : summary(sum)
        , emotional_tag(tag)
        , importance(imp)
        , timestamp(std::chrono::system_clock::now())
    {}
};

/**
 * @brief 記憶コントローラー (Memory Controller)
 * 
 * 「物語的自己形成」を司るデータベース管理層。
 * 短期メモリと長期メモリを管理し、関連する記憶を検索します。
 */
class MemoryController {
public:
    MemoryController();
    explicit MemoryController(int short_term_limit);
    ~MemoryController();

    // ===== 短期メモリ（直近の会話） =====
    
    /**
     * @brief 会話ターンを短期メモリに追加
     * @param role "user" または "assistant"
     * @param content 発言内容
     */
    void add_to_short_term(const std::string& role, const std::string& content);

    /**
     * @brief 短期メモリをクリア
     */
    void clear_short_term();

    /**
     * @brief 短期メモリの全ての会話を取得
     * @return 会話履歴
     */
    const std::deque<ConversationTurn>& get_short_term_history() const {
        return short_term_memory_;
    }

    /**
     * @brief 短期メモリをテキスト形式で取得
     * @param max_turns 取得する最大ターン数（0 = 全て）
     * @return フォーマットされた会話履歴
     */
    std::string get_short_term_as_text(int max_turns = 0) const;

    // ===== 長期メモリ（物語的自己） =====

    /**
     * @brief エピソードを長期メモリに追加
     * @param episode 追加するエピソード
     */
    void add_to_long_term(const Episode& episode);

    /**
     * @brief キーワードに基づいて関連エピソードを検索
     * @param keywords 検索キーワード
     * @param max_results 取得する最大件数
     * @return 関連するエピソードのリスト
     */
    std::vector<Episode> search_episodes(
        const std::vector<std::string>& keywords,
        int max_results = 5) const;

    /**
     * @brief 感情タグに基づいてエピソードを検索
     * @param emotional_tag 感情タグ（"positive", "negative", "neutral"）
     * @param max_results 取得する最大件数
     * @return 該当するエピソードのリスト
     */
    std::vector<Episode> search_by_emotion(
        const std::string& emotional_tag,
        int max_results = 5) const;

    /**
     * @brief 長期メモリの全エピソードを取得
     * @return 全エピソード
     */
    const std::vector<Episode>& get_all_episodes() const {
        return long_term_memory_;
    }

    /**
     * @brief 長期メモリをクリア
     */
    void clear_long_term();

    /**
     * @brief 短期メモリから重要なエピソードを抽出して長期メモリに保存
     * @param emotional_state 現在の感情状態の説明
     * @param keywords 関連キーワード
     * @param summary_override 要約文の上書き（空文字なら内部要約を使用）
     */
    void consolidate_memory(
        const std::string& emotional_state,
        const std::vector<std::string>& keywords,
        const std::string& summary_override = "");

    // ===== 統計・管理機能 =====

    /**
     * @brief 短期メモリのサイズを取得
     * @return ターン数
     */
    int get_short_term_size() const { return static_cast<int>(short_term_memory_.size()); }

    /**
     * @brief 長期メモリのサイズを取得
     * @return エピソード数
     */
    int get_long_term_size() const { return static_cast<int>(long_term_memory_.size()); }

    /**
     * @brief 短期メモリの最大サイズを設定
     * @param limit 最大ターン数
     */
    void set_short_term_limit(int limit) { short_term_limit_ = limit; }

    /**
     * @brief 短期メモリの最大サイズを取得
     * @return 最大ターン数
     */
    int get_short_term_limit() const { return short_term_limit_; }

    /**
     * @brief 短期メモリ履歴を一括復元
     * @param history 復元する履歴
     */
    void set_short_term_history(const std::deque<ConversationTurn>& history);

    /**
     * @brief 長期メモリを一括復元
     * @param episodes 復元するエピソード一覧
     */
    void set_long_term_memory(const std::vector<Episode>& episodes);

private:
    std::deque<ConversationTurn> short_term_memory_;  // 短期メモリ（FIFO）
    std::vector<Episode> long_term_memory_;           // 長期メモリ
    int short_term_limit_;                            // 短期メモリの最大サイズ

    /**
     * @brief キーワードの類似度を計算（簡易実装）
     * @param keywords1 キーワードセット1
     * @param keywords2 キーワードセット2
     * @return 類似度スコア（0.0 ~ 1.0）
     */
    double calculate_similarity(
        const std::vector<std::string>& keywords1,
        const std::vector<std::string>& keywords2) const;

    /**
     * @brief エピソードの重要度を計算
     * @param conversation_length 会話の長さ
     * @param has_strong_emotion 強い感情があるか
     * @return 重要度スコア（0.0 ~ 1.0）
     */
    double calculate_importance(int conversation_length, bool has_strong_emotion) const;
};
