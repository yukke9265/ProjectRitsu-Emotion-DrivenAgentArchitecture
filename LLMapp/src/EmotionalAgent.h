#pragma once

#include "InputAnalyzer.h"
#include "ToolAnalyzer.h"
#include "EmotionEngine.h"
#include "MemoryController.h"
#include "PromptOrchestrator.h"
#include "LLMInference.h"
#include "ToolIO.h"
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
     * @brief システムプロンプトへ挿入する構造化ログを1件追加
     * @param section_name セクション名
     * @param content ログ本文
     */
    void add_system_log_section(const std::string& section_name, const std::string& content);

    /**
     * @brief システムプロンプトへ挿入する構造化ログを全てクリア
     */
    void clear_system_log_sections();

    /**
     * @brief 人格憲法を設定
     * @param constitution 新しい人格憲法
     */
    void set_constitution(const PersonalityConstitution& constitution);

    /**
     * @brief デバッグモードを有効化/無効化
     * @param enable trueでデバッグログを出力
     */
    void set_debug_mode(bool enable);

    /**
     * @brief ツール実行器を設定（非所有ポインタ）
     *
     * 使い方:
     * @code
     * ToolRegistryExecutor registry;
     * registry.register_tool(...);
     * agent.set_tool_executor(&registry);
     * @endcode
     */
    void set_tool_executor(IToolExecutor* executor);

    /**
     * @brief 内蔵ツールレジストリへツールを追加
     *
     * 外部実行器を使わない場合はこのAPIだけで登録可能。
     */
    void register_tool(const ToolSpec& spec, ToolRegistryExecutor::ToolHandler handler);

    /**
     * @brief スキーマ付きで内蔵ツールを登録
     */
    void register_tool(
        const ToolSpec& spec,
        const ToolObjectSchema& schema,
        ToolRegistryExecutor::ToolHandler handler);

    /**
     * @brief LLMによるツール利用を有効/無効化
     */
    void set_tool_use_enabled(bool enable) { tool_use_enabled_ = enable; }

    /**
     * @brief 1ターン内の最大ツール呼び出し回数を設定
     */
    void set_max_tool_iterations(int max_iterations);

    /**
     * @brief デバッグモードの状態を取得
     * @return デバッグモードが有効ならtrue
     */
    bool is_debug_mode() const { return debug_mode_; }

    /**
     * @brief 会話履歴をクリア
     */
    void clear_history();

    /**
     * @brief エージェントをリセット（感情と記憶をクリア）
     */
    void reset();

    /**
     * @brief エージェント状態をファイルへ保存
     * @param file_path 保存先ファイルパス
     * @return 成功した場合true
     */
    bool save_state_to_file(const std::string& file_path) const;

    /**
     * @brief エージェント状態をファイルから復元
     * @param file_path 読込元ファイルパス
     * @return 成功した場合true
     */
    bool load_state_from_file(const std::string& file_path);

    /**
     * @brief デバッグ情報を出力
     */
    void print_debug_info() const;

    /**
     * @brief 状態を更新せずに最終プロンプトを生成して取得
     * @param user_input プロンプト生成時のユーザー入力（省略可）
     * @param phase プロンプト生成フェーズ（Tool/Response）
     * @return 生成された最終プロンプト
     */
    std::string build_prompt_preview(
        const std::string& user_input = "",
        PromptOrchestrator::PromptPhase phase = PromptOrchestrator::PromptPhase::Response);

private:
    // 5つのモジュール
    std::unique_ptr<InputAnalyzer> input_analyzer_;
    std::unique_ptr<ToolAnalyzer> tool_analyzer_;
    std::unique_ptr<EmotionEngine> emotion_engine_;
    std::unique_ptr<MemoryController> memory_controller_;
    std::unique_ptr<PromptOrchestrator> prompt_orchestrator_;
    std::unique_ptr<LLMInference> llm_inference_;

    // LLMパラメータ
    std::string model_path_;
    bool initialized_;
    std::string last_error_;
    bool debug_mode_;
    bool tool_use_enabled_;
    int max_tool_iterations_;

    // ツール実行器（set_tool_executor()で外部注入可能）
    IToolExecutor* tool_executor_;

    // 内蔵レジストリ（register_tool()使用時に利用）
    std::unique_ptr<ToolRegistryExecutor> owned_tool_registry_;

    /**
     * @brief 記憶の統合を実行（定期的に呼ばれる）
     */
    void consolidate_memories();

    /**
     * @brief 直近会話をLLMで要約
     * @return 要約文（失敗時は空文字）
     */
    std::string summarize_recent_conversation_with_llm() const;
};
