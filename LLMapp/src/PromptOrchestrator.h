#pragma once

#include "EmotionEngine.h"
#include "MemoryController.h"
#include <string>
#include <vector>

/**
 * @brief プロンプト・オーケストレーター (Prompt Orchestrator)
 * 
 * 各モジュールの情報を、最終的な「システムプロンプト」へ合成する組み立て役。
 * 「人格憲法（固定）」＋「最新の感情数値（動的）」＋「関連する過去の記憶」を統合し、
 * LLMが解釈しやすいマークダウン形式に整形します。
 */
class PromptOrchestrator {
public:
    struct StructuredLogSection {
        std::string name;
        std::string content;
    };

    PromptOrchestrator();
    ~PromptOrchestrator();

    /**
     * @brief 最終的なプロンプトを生成
     * @param user_input ユーザーの入力
     * @param emotion_engine 感情エンジン
     * @param memory_controller 記憶コントローラー
     * @return 生成されたプロンプト
     */
    std::string build_final_prompt(
        const std::string& user_input,
        const EmotionEngine& emotion_engine,
        const MemoryController& memory_controller);

    /**
     * @brief システムプロンプト（人格憲法）を設定
     * @param system_prompt システムプロンプト
     */
    void set_system_prompt(const std::string& system_prompt);

    const std::string& get_system_prompt() const { return system_prompt_; }

    /**
     * @brief 応答トーン制御の指示を設定
     * @param tone_instruction トーン制御の指示
     */
    void set_tone_instruction(const std::string& tone_instruction);

    const std::string& get_tone_instruction() const { return tone_instruction_; }

    /**
     * @brief 返答スタイル指示を設定
     * @param response_style_instruction 返答スタイル指示
     */
    void set_response_style_instruction(const std::string& response_style_instruction);

    const std::string& get_response_style_instruction() const { return response_style_instruction_; }

    /**
     * @brief システムプロンプトに挿入する構造化ログセクションを追加
     * @param section_name セクション名
     * @param content ログ本文
     */
    void add_system_log_section(const std::string& section_name, const std::string& content);

    /**
     * @brief システムログセクションを全てクリア
     */
    void clear_system_log_sections();

    /**
     * @brief システムログセクションを一括置換
     * @param sections 新しいログセクション一覧
     */
    void set_system_log_sections(const std::vector<StructuredLogSection>& sections);

    const std::vector<StructuredLogSection>& get_system_log_sections() const { return system_log_sections_; }

    /**
     * @brief 記憶検索の最大件数を設定
     * @param max_episodes 最大エピソード数
     */
    void set_max_episodes(int max_episodes) { max_episodes_ = max_episodes; }

    int get_max_episodes() const { return max_episodes_; }

    /**
     * @brief 短期メモリの表示ターン数を設定
     * @param max_turns 最大ターン数（0 = 全て）
     */
    void set_short_term_turns(int max_turns) { short_term_turns_ = max_turns; }

    int get_short_term_turns() const { return short_term_turns_; }

private:
    std::string system_prompt_;      // 基本システムプロンプト
    std::string tone_instruction_;   // トーン制御の指示
    std::string response_style_instruction_; // 返答スタイル指示
    std::vector<StructuredLogSection> system_log_sections_; // 構造化システムログ
    int max_episodes_;               // 記憶検索の最大件数
    int short_term_turns_;           // 短期メモリの表示ターン数

    /**
     * @brief 構造化システムログをプロンプト用テキストに変換
     * @return システムログのテキスト
     */
    std::string format_system_logs() const;

    /**
     * @brief 指定セクションを除外して構造化システムログをプロンプト用テキストに変換
     * @param excluded_section_name 除外するセクション名
     * @return システムログのテキスト
     */
    std::string format_system_logs_excluding(const std::string& excluded_section_name) const;

    /**
     * @brief 指定名のシステムログセクション本文を取得
     * @param section_name セクション名
     * @return セクション本文（未存在時は空文字）
     */
    std::string find_system_log_section(const std::string& section_name) const;

    /**
     * @brief 感情状態をプロンプト用テキストに変換
     * @param emotion_engine 感情エンジン
     * @return 感情状態の説明文
     */
    std::string format_emotion_state(const EmotionEngine& emotion_engine);

    /**
     * @brief 短期メモリをプロンプト用テキストに変換
     * @param memory_controller 記憶コントローラー
     * @return 会話履歴のテキスト
     */
    std::string format_short_term_memory(const MemoryController& memory_controller);

    /**
     * @brief 関連する長期記憶をプロンプト用テキストに変換
     * @param memory_controller 記憶コントローラー
     * @param keywords 検索キーワード
     * @return 関連エピソードのテキスト
     */
    std::string format_long_term_memory(
        const MemoryController& memory_controller,
        const std::vector<std::string>& keywords);

    /**
     * @brief トーン制御の指示を生成
     * @param emotion_state 感情状態
     * @return トーン制御の指示文
     */
    std::string generate_tone_control(const EmotionState& emotion_state);
};
