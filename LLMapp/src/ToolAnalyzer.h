#pragma once

#include <string>
#include <vector>
#include <deque>

// 前方宣言
class LLMInference;
struct ToolSpec;
struct ConversationTurn;

/**
 * @brief 使用ツール分析結果
 */
struct AnalyzedToolSet {
    std::vector<std::string> tool_names;  // 推奨ツール名一覧
    std::string reason;                   // 推奨の理由
    bool needs_tool_call;                 // ツール呼び出しが必要か

    AnalyzedToolSet()
        : needs_tool_call(true)
    {}
};

/**
 * @brief ツール分析モジュール (Tool Analyzer)
 *
 * ユーザーの発言と会話履歴から、このターンで必要なツールを推定します。
 * InputAnalyzer と同様に、LLMベースで判定して動的にツール説明をフィルタリング。
 */
class ToolAnalyzer {
public:
    ToolAnalyzer();
    ~ToolAnalyzer();

    /**
     * @brief LLMインスタンスを設定
     * @param llm LLMInferenceへのポインタ
     */
    void set_llm_inference(LLMInference* llm);

    /**
     * @brief デバッグモードを有効化/無効化
     * @param enable trueでデバッグログを出力
     */
    void set_debug_mode(bool enable) { debug_mode_ = enable; }

    /**
     * @brief 必要なツールを分析（std::vector版）
     * @param user_input ユーザー発言
     * @param available_tools 利用可能なツール一覧
     * @param conversation_history 会話履歴
     * @return 推奨ツールセット
     */
    AnalyzedToolSet analyze(
        const std::string& user_input,
        const std::vector<ToolSpec>& available_tools,
        const std::vector<ConversationTurn>* conversation_history = nullptr
    );

    /**
     * @brief 必要なツールを分析（std::deque版）
     * @param user_input ユーザー発言
     * @param available_tools 利用可能なツール一覧
     * @param conversation_history 会話履歴
     * @return 推奨ツールセット
     */
    AnalyzedToolSet analyze(
        const std::string& user_input,
        const std::vector<ToolSpec>& available_tools,
        const std::deque<ConversationTurn>* conversation_history
    );

private:
    LLMInference* llm_inference_;
    bool debug_mode_;

    // LLMで必要ツールを判定するプロンプト生成
    std::string build_tool_analysis_prompt(
        const std::string& user_input,
        const std::vector<ToolSpec>& available_tools,
        const std::deque<ConversationTurn>* conversation_history
    );
};
