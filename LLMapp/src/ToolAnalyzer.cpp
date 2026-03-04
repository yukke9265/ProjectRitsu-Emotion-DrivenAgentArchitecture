#include "ToolAnalyzer.h"
#include "LLMInference.h"
#include "ToolIO.h"
#include "MemoryController.h" // 追加: ConversationTurnの完全な型定義を得るため
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream> // 追加

namespace {
    std::string trim_copy(const std::string& text) {
        auto is_ws = [](unsigned char c) { return std::isspace(c) != 0; };
        auto first = std::find_if_not(text.begin(), text.end(), is_ws);
        if (first == text.end()) return "";
        auto last = std::find_if_not(text.rbegin(), text.rend(), is_ws).base();
        return std::string(first, last);
    }
}

ToolAnalyzer::ToolAnalyzer()
    : llm_inference_(nullptr)
    , debug_mode_(false)
{
}

ToolAnalyzer::~ToolAnalyzer()
{
}

void ToolAnalyzer::set_llm_inference(LLMInference* llm)
{
    llm_inference_ = llm;
}

AnalyzedToolSet ToolAnalyzer::analyze(
    const std::string& user_input,
    const std::vector<ToolSpec>& available_tools,
    const std::vector<ConversationTurn>* conversation_history)
{
    AnalyzedToolSet result;

    if (!llm_inference_ || available_tools.empty()) {
        result.needs_tool_call = false;
        result.reason = "LLM not available or no tools registered";
        return result;
    }

    std::deque<ConversationTurn> history_deque;
    const std::deque<ConversationTurn>* history_ptr = nullptr;
    if (conversation_history) {
        history_deque.assign(conversation_history->begin(), conversation_history->end());
        history_ptr = &history_deque;
    }

    const std::string analysis_prompt = build_tool_analysis_prompt(
        user_input,
        available_tools,
        history_ptr
    );

    const std::string llm_response = llm_inference_->infer(analysis_prompt);

    if (debug_mode_) {
        std::cout << "[ToolAnalyzer] LLM分析結果:\n" << llm_response << "\n\n";
    }

    // 簡易パース: "ツール: name1, name2" または "不要" という形式を想定
    std::string lower_response = llm_response;
    std::transform(lower_response.begin(), lower_response.end(), lower_response.begin(),
        [](unsigned char c) { return std::tolower(c); });

    if (lower_response.find("不要") != std::string::npos ||
        lower_response.find("toolは不要") != std::string::npos ||
        lower_response.find("tool不要") != std::string::npos) {
        result.needs_tool_call = false;
        result.reason = "ユーザーの発言からはツール呼び出しが不要と判定されました";
        return result;
    }

    // ツール名抽出（単純な方法：利用可能なツール名を検索）
    result.needs_tool_call = true;
    for (const auto& tool : available_tools) {
        if (llm_response.find(tool.name) != std::string::npos) {
            result.tool_names.push_back(tool.name);
        }
    }

    // ツール名が見つからない場合は、すべて利用可能にする（安全策）
    if (result.tool_names.empty()) {
        for (const auto& tool : available_tools) {
            if (tool.name != "finish_tool_planning") {
                result.tool_names.push_back(tool.name);
            }
        }
        result.reason = "複数のツールが必要である可能性があります";
    } else {
        std::ostringstream oss;
        oss << "推奨ツール: ";
        for (size_t i = 0; i < result.tool_names.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << result.tool_names[i];
        }
        result.reason = oss.str();
    }

    return result;
}

// std::deque版のオーバーロード
AnalyzedToolSet ToolAnalyzer::analyze(
    const std::string& user_input,
    const std::vector<ToolSpec>& available_tools,
    const std::deque<ConversationTurn>* conversation_history)
{
    AnalyzedToolSet result;

    if (!llm_inference_ || available_tools.empty()) {
        result.needs_tool_call = false;
        result.reason = "LLM not available or no tools registered";
        return result;
    }

    const std::string analysis_prompt = build_tool_analysis_prompt(
        user_input,
        available_tools,
        conversation_history
    );

    const std::string llm_response = llm_inference_->infer(analysis_prompt);

    if (debug_mode_) {
        std::cout << "[ToolAnalyzer] LLM分析結果:\n" << llm_response << "\n\n";
    }

    // 簡易パース: "ツール: name1, name2" または "不要" という形式を想定
    std::string lower_response = llm_response;
    std::transform(lower_response.begin(), lower_response.end(), lower_response.begin(),
        [](unsigned char c) { return std::tolower(c); });

    if (lower_response.find("不要") != std::string::npos ||
        lower_response.find("toolは不要") != std::string::npos ||
        lower_response.find("tool不要") != std::string::npos) {
        result.needs_tool_call = false;
        result.reason = "ユーザーの発言からはツール呼び出しが不要と判定されました";
        return result;
    }

    // ツール名抽出（単純な方法：利用可能なツール名を検索）
    result.needs_tool_call = true;
    for (const auto& tool : available_tools) {
        if (llm_response.find(tool.name) != std::string::npos) {
            result.tool_names.push_back(tool.name);
        }
    }

    // ツール名が見つからない場合は、すべて利用可能にする（安全策）
    if (result.tool_names.empty()) {
        for (const auto& tool : available_tools) {
            if (tool.name != "finish_tool_planning") {
                result.tool_names.push_back(tool.name);
            }
        }
        result.reason = "複数のツールが必要である可能性があります";
    } else {
        std::ostringstream oss;
        oss << "推奨ツール: ";
        for (size_t i = 0; i < result.tool_names.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << result.tool_names[i];
        }
        result.reason = oss.str();
    }

    return result;
}

std::string ToolAnalyzer::build_tool_analysis_prompt(
    const std::string& user_input,
    const std::vector<ToolSpec>& available_tools,
    const std::deque<ConversationTurn>* conversation_history)
{
    std::ostringstream oss;

    oss << "以下は利用可能なツール一覧です:\n\n";
    for (const auto& tool : available_tools) {
        oss << "- " << tool.name << ": " << tool.description << "\n";
    }

    oss << "\n【タスク】\n";
    oss << "ユーザーの発言を見て、このターンで呼び出すべきツールを判定してください。\n\n";

    if (conversation_history && !conversation_history->empty()) {
        oss << "最近の会話履歴:\n";
        size_t start = conversation_history->size() > 3
            ? conversation_history->size() - 3
            : 0;
        for (size_t i = start; i < conversation_history->size(); ++i) {
            oss << (*conversation_history)[i].role << ": "
                << (*conversation_history)[i].content << "\n";
        }
        oss << "\n";
    }

    oss << "ユーザー発言: \"" << user_input << "\"\n\n";

    oss << "【回答形式】\n";
    oss << "必要なツール名をカンマ区切りで列挙してください。\n";
    oss << "例1: number_guess_game, get_current_time\n";
    oss << "例2: ツールは不要\n";
    oss << "例3: get_current_time のみ必要\n\n";

    oss << "【回答】\n";

    return oss.str();
}
