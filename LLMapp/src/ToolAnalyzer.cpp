#include "ToolAnalyzer.h"
#include "LLMInference.h"
#include "ToolIO.h"
#include "MemoryController.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace {
    std::string trim_copy(const std::string& text) {
        auto is_ws = [](unsigned char c) { return std::isspace(c) != 0; };
        auto first = std::find_if_not(text.begin(), text.end(), is_ws);
        if (first == text.end()) return "";
        auto last = std::find_if_not(text.rbegin(), text.rend(), is_ws).base();
        return std::string(first, last);
    }

    std::string find_tool_input_schema(
        const std::vector<ToolSpec>& available_tools,
        const std::string& tool_name) {
        for (const auto& tool : available_tools) {
            if (tool.name == tool_name) {
                return tool.input_schema;
            }
        }
        return "";
    }

    std::string build_tool_usage_hint(const std::string& tool_name, const std::string& input_schema) {
        std::ostringstream oss;
        if (!input_schema.empty()) {
            oss << "使い方: " << input_schema;
        } else {
            oss << "使い方: help ツールを tool_call して {\"tool\":\"" << tool_name << "\"} を確認";
        }
        return oss.str();
    }

    std::string to_lower_copy(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    bool is_tool_name_char(unsigned char c) {
        return std::isalnum(c) != 0 || c == '_';
    }

    bool contains_tool_name_token(const std::string& lower_text, const std::string& tool_name) {
        if (tool_name.empty()) {
            return false;
        }

        const std::string needle = to_lower_copy(tool_name);
        size_t pos = lower_text.find(needle);
        while (pos != std::string::npos) {
            const bool left_ok = (pos == 0) || !is_tool_name_char(static_cast<unsigned char>(lower_text[pos - 1]));
            const size_t right = pos + needle.size();
            const bool right_ok = (right >= lower_text.size()) || !is_tool_name_char(static_cast<unsigned char>(lower_text[right]));
            if (left_ok && right_ok) {
                return true;
            }
            pos = lower_text.find(needle, pos + 1);
        }

        return false;
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
    std::deque<ConversationTurn> history_deque;
    const std::deque<ConversationTurn>* history_ptr = nullptr;
    if (conversation_history) {
        history_deque.assign(conversation_history->begin(), conversation_history->end());
        history_ptr = &history_deque;
    }
    return analyze_with_history_deque(user_input, available_tools, history_ptr);
}

AnalyzedToolSet ToolAnalyzer::analyze(
    const std::string& user_input,
    const std::vector<ToolSpec>& available_tools,
    const std::deque<ConversationTurn>* conversation_history)
{
    return analyze_with_history_deque(user_input, available_tools, conversation_history);
}

AnalyzedToolSet ToolAnalyzer::analyze_with_history_deque(
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

    const std::string lower_response = to_lower_copy(llm_response);

    if (lower_response.find("不要") != std::string::npos ||
        lower_response.find("toolは不要") != std::string::npos ||
        lower_response.find("tool不要") != std::string::npos) {
        result.needs_tool_call = false;
        result.reason = "ユーザーの発言からはツール呼び出しが不要と判定されました";
        return result;
    }

    result.needs_tool_call = true;
    for (const auto& tool : available_tools) {
        if (contains_tool_name_token(lower_response, tool.name)) {
            result.tool_names.push_back(tool.name);
        }
    }

    if (result.tool_names.empty()) {
        bool has_help = false;
        for (const auto& tool : available_tools) {
            if (tool.name == "help") {
                has_help = true;
                break;
            }
        }

        if (has_help) {
            result.tool_names.push_back("help");
            result.reason = "推奨ツールを特定できなかったため help を優先して使い方を確認します。\n"
                "- help / 使い方: {\"tool\":\"<tool_name>\"}（未指定で一覧）";
            return result;
        }

        for (const auto& tool : available_tools) {
            if (tool.name != "finish_tool_planning") {
                result.tool_names.push_back(tool.name);
            }
        }
        std::ostringstream oss;
        oss << "複数のツールが必要である可能性があります。\n";
        for (const auto& tool_name : result.tool_names) {
            const std::string schema = find_tool_input_schema(available_tools, tool_name);
            oss << "- " << tool_name << " / " << build_tool_usage_hint(tool_name, schema) << "\n";
        }
        result.reason = trim_copy(oss.str());
    } else {
        std::ostringstream oss;
        oss << "推奨ツール: ";
        for (size_t i = 0; i < result.tool_names.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << result.tool_names[i];
        }
        oss << "\n";
        for (const auto& tool_name : result.tool_names) {
            const std::string schema = find_tool_input_schema(available_tools, tool_name);
            oss << "- " << tool_name << " / " << build_tool_usage_hint(tool_name, schema) << "\n";
        }
        result.reason = trim_copy(oss.str());
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

    oss << "\n補足: ツールの詳細な使い方が必要な場合は help ツールを選択してください。\n";

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
