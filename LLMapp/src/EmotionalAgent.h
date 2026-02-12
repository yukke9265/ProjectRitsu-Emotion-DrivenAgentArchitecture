#pragma once

#include "InputAnalyzer.h"
#include "EmotionEngine.h"
#include "MemoryController.h"
#include "PromptOrchestrator.h"
#include "LLMInference.h"
#include <string>
#include <memory>

/**
 * @brief 感情駆動型AIエージェント
 * 
 * 5つのモジュールを統合したエージェントクラス。
 * 1. InputAnalyzer - 入力解析
 * 2. EmotionEngine - 感情管理
 * 3. MemoryController - 記憶管理
 * 4. PromptOrchestrator - プロンプト生成
 * 5. LLMInference - 文章生成
 */
class EmotionalAgent {
public:
    /**
     * @brief コンストラクタ
     * @param model_path LLMモデルのパス
     * @param constitution 人格憲法（省略可）
     */
    explicit EmotionalAgent(
        const std::string& model_path,
        const PersonalityConstitution& constitution = PersonalityConstitution());

    ~EmotionalAgent();

    /**
     * @brief エージェントを初期化
     * @return 成功した場合true
     */
    bool initialize();

    /**
     * @brief ユーザー入力を処理して応答を生成
     * @param user_input ユーザーの発言
     * @return AIの応答
     */
    std::string process(const std::string& user_input);

    /**
     * @brief 現在の感情状態を取得
     * @return 感情状態の説明
     */
    std::string get_emotion_status() const;

    /**
     * @brief 会話履歴を取得
     * @param max_turns 取得する最大ターン数
     * @return 会話履歴のテキスト
     */
    std::string get_conversation_history(int max_turns = 0) const;

    /**
     * @brief 長期記憶のエピソード数を取得
     * @return エピソード数
     */
    int get_episode_count() const;

    /**
     * @brief システムプロンプトを設定
     * @param system_prompt システムプロンプト
     */
    void set_system_prompt(const std::string& system_prompt);

    /**
     * @brief 人格憲法を設定
     * @param constitution 新しい人格憲法
     */
    void set_constitution(const PersonalityConstitution& constitution);

    /**
     * @brief 会話履歴をクリア
     */
    void clear_history();

    /**
     * @brief エージェントをリセット（感情と記憶をクリア）
     */
    void reset();

    /**
     * @brief デバッグ情報を出力
     */
    void print_debug_info() const;

private:
    // 5つのモジュール
    std::unique_ptr<InputAnalyzer> input_analyzer_;
    std::unique_ptr<EmotionEngine> emotion_engine_;
    std::unique_ptr<MemoryController> memory_controller_;
    std::unique_ptr<PromptOrchestrator> prompt_orchestrator_;
    std::unique_ptr<LLMInference> llm_inference_;

    // LLMパラメータ
    std::string model_path_;
    bool initialized_;
    std::string last_error_;

    /**
     * @brief 記憶の統合を実行（定期的に呼ばれる）
     */
    void consolidate_memories();
};
