#include "ToolIO.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace {

class JsonParser {
public:
    explicit JsonParser(const std::string& text)
        : text_(text), pos_(0) {
    }

    bool parse(ToolJsonValue& out_value, std::string& out_error) {
        skip_ws();
        if (!parse_value(out_value, out_error)) {
            return false;
        }
        skip_ws();
        if (!eof()) {
            out_error = "unexpected trailing characters at position " + std::to_string(pos_);
            return false;
        }
        return true;
    }

private:
    const std::string& text_;
    size_t pos_;

    bool eof() const {
        return pos_ >= text_.size();
    }

    char peek() const {
        return eof() ? '\0' : text_[pos_];
    }

    char get() {
        return eof() ? '\0' : text_[pos_++];
    }

    void skip_ws() {
        while (!eof() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }

    bool parse_value(ToolJsonValue& out, std::string& err) {
        skip_ws();
        if (eof()) {
            err = "unexpected end of input";
            return false;
        }

        const char c = peek();
        if (c == '{') {
            return parse_object(out, err);
        }
        if (c == '[') {
            return parse_array(out, err);
        }
        if (c == '"') {
            std::string s;
            if (!parse_string(s, err)) {
                return false;
            }
            out.value = std::move(s);
            return true;
        }
        if (c == 't') {
            return parse_true(out, err);
        }
        if (c == 'f') {
            return parse_false(out, err);
        }
        if (c == 'n') {
            return parse_null(out, err);
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0) {
            return parse_number(out, err);
        }

        err = "unexpected token at position " + std::to_string(pos_);
        return false;
    }

    bool parse_object(ToolJsonValue& out, std::string& err) {
        ToolJsonValue::Object object;

        if (get() != '{') {
            err = "expected '{'";
            return false;
        }

        skip_ws();
        if (peek() == '}') {
            get();
            out.value = std::move(object);
            return true;
        }

        while (true) {
            skip_ws();
            if (peek() != '"') {
                err = "expected string key at position " + std::to_string(pos_);
                return false;
            }

            std::string key;
            if (!parse_string(key, err)) {
                return false;
            }

            skip_ws();
            if (get() != ':') {
                err = "expected ':' after key at position " + std::to_string(pos_);
                return false;
            }

            ToolJsonValue value;
            if (!parse_value(value, err)) {
                return false;
            }

            object[key] = std::move(value);

            skip_ws();
            const char c = get();
            if (c == '}') {
                break;
            }
            if (c != ',') {
                err = "expected ',' or '}' at position " + std::to_string(pos_);
                return false;
            }
        }

        out.value = std::move(object);
        return true;
    }

    bool parse_array(ToolJsonValue& out, std::string& err) {
        ToolJsonValue::Array array;

        if (get() != '[') {
            err = "expected '['";
            return false;
        }

        skip_ws();
        if (peek() == ']') {
            get();
            out.value = std::move(array);
            return true;
        }

        while (true) {
            ToolJsonValue value;
            if (!parse_value(value, err)) {
                return false;
            }

            array.push_back(std::move(value));

            skip_ws();
            const char c = get();
            if (c == ']') {
                break;
            }
            if (c != ',') {
                err = "expected ',' or ']' at position " + std::to_string(pos_);
                return false;
            }
        }

        out.value = std::move(array);
        return true;
    }

    bool parse_string(std::string& out, std::string& err) {
        if (get() != '"') {
            err = "expected '\"'";
            return false;
        }

        std::ostringstream oss;
        while (!eof()) {
            const char c = get();
            if (c == '"') {
                out = oss.str();
                return true;
            }

            if (c == '\\') {
                if (eof()) {
                    err = "unterminated escape sequence";
                    return false;
                }

                const char e = get();
                switch (e) {
                case '"': oss << '"'; break;
                case '\\': oss << '\\'; break;
                case '/': oss << '/'; break;
                case 'b': oss << '\b'; break;
                case 'f': oss << '\f'; break;
                case 'n': oss << '\n'; break;
                case 'r': oss << '\r'; break;
                case 't': oss << '\t'; break;
                default:
                    err = "unsupported escape sequence at position " + std::to_string(pos_);
                    return false;
                }
                continue;
            }

            oss << c;
        }

        err = "unterminated string";
        return false;
    }

    bool parse_number(ToolJsonValue& out, std::string& err) {
        const size_t start = pos_;

        if (peek() == '-') {
            get();
        }

        if (!std::isdigit(static_cast<unsigned char>(peek()))) {
            err = "invalid number at position " + std::to_string(pos_);
            return false;
        }

        if (peek() == '0') {
            get();
        }
        else {
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                get();
            }
        }

        if (peek() == '.') {
            get();
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                err = "invalid fractional part at position " + std::to_string(pos_);
                return false;
            }
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                get();
            }
        }

        if (peek() == 'e' || peek() == 'E') {
            get();
            if (peek() == '+' || peek() == '-') {
                get();
            }
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                err = "invalid exponent at position " + std::to_string(pos_);
                return false;
            }
            while (std::isdigit(static_cast<unsigned char>(peek())) != 0) {
                get();
            }
        }

        const std::string number_text = text_.substr(start, pos_ - start);
        char* end = nullptr;
        const double value = std::strtod(number_text.c_str(), &end);
        if (end == nullptr || *end != '\0') {
            err = "invalid number format";
            return false;
        }
        if (value == HUGE_VAL || value == -HUGE_VAL) {
            err = "number out of range";
            return false;
        }

        out.value = value;
        return true;
    }

    bool parse_true(ToolJsonValue& out, std::string& err) {
        if (text_.compare(pos_, 4, "true") != 0) {
            err = "expected 'true' at position " + std::to_string(pos_);
            return false;
        }
        pos_ += 4;
        out.value = true;
        return true;
    }

    bool parse_false(ToolJsonValue& out, std::string& err) {
        if (text_.compare(pos_, 5, "false") != 0) {
            err = "expected 'false' at position " + std::to_string(pos_);
            return false;
        }
        pos_ += 5;
        out.value = false;
        return true;
    }

    bool parse_null(ToolJsonValue& out, std::string& err) {
        if (text_.compare(pos_, 4, "null") != 0) {
            err = "expected 'null' at position " + std::to_string(pos_);
            return false;
        }
        pos_ += 4;
        out.value = nullptr;
        return true;
    }
};

}

bool ToolJsonValue::is_null() const {
    return std::holds_alternative<std::nullptr_t>(value);
}

bool ToolJsonValue::is_bool() const {
    return std::holds_alternative<bool>(value);
}

bool ToolJsonValue::is_number() const {
    return std::holds_alternative<double>(value);
}

bool ToolJsonValue::is_string() const {
    return std::holds_alternative<std::string>(value);
}

bool ToolJsonValue::is_object() const {
    return std::holds_alternative<Object>(value);
}

bool ToolJsonValue::is_array() const {
    return std::holds_alternative<Array>(value);
}

std::optional<bool> ToolJsonValue::as_bool() const {
    if (!is_bool()) {
        return std::nullopt;
    }
    return std::get<bool>(value);
}

std::optional<double> ToolJsonValue::as_number() const {
    if (!is_number()) {
        return std::nullopt;
    }
    return std::get<double>(value);
}

std::optional<std::string> ToolJsonValue::as_string() const {
    if (!is_string()) {
        return std::nullopt;
    }
    return std::get<std::string>(value);
}

const ToolJsonValue::Object* ToolJsonValue::as_object() const {
    return std::get_if<Object>(&value);
}

const ToolJsonValue::Array* ToolJsonValue::as_array() const {
    return std::get_if<Array>(&value);
}

bool ToolJsonInput::parse(const std::string& text) {
    error_message_.clear();

    if (text.empty()) {
        root_.value = nullptr;
        error_message_ = "empty_input";
        return false;
    }

    JsonParser parser(text);
    if (!parser.parse(root_, error_message_)) {
        return false;
    }

    return true;
}

namespace {

std::string tool_json_type_to_string(ToolJsonType type) {
    switch (type) {
    case ToolJsonType::ANY: return "any";
    case ToolJsonType::NULL_VALUE: return "null";
    case ToolJsonType::BOOL: return "bool";
    case ToolJsonType::NUMBER: return "number";
    case ToolJsonType::STRING: return "string";
    case ToolJsonType::OBJECT: return "object";
    case ToolJsonType::ARRAY: return "array";
    }
    return "unknown";
}

bool value_matches_type(const ToolJsonValue& value, ToolJsonType type) {
    switch (type) {
    case ToolJsonType::ANY: return true;
    case ToolJsonType::NULL_VALUE: return value.is_null();
    case ToolJsonType::BOOL: return value.is_bool();
    case ToolJsonType::NUMBER: return value.is_number();
    case ToolJsonType::STRING: return value.is_string();
    case ToolJsonType::OBJECT: return value.is_object();
    case ToolJsonType::ARRAY: return value.is_array();
    }
    return false;
}

std::string describe_object_schema(const ToolObjectSchema& schema) {
    std::ostringstream oss;
    oss << "JSON object: { ";
    for (size_t i = 0; i < schema.fields.size(); ++i) {
        const auto& field = schema.fields[i];
        if (i > 0) {
            oss << ", ";
        }
        oss << "\"" << field.key << "\"";
        if (!field.required) {
            oss << "?";
        }
        oss << ":" << tool_json_type_to_string(field.type);
    }
    oss << " }";
    if (!schema.allow_additional_keys) {
        oss << " (additional keys: forbidden)";
    }
    return oss.str();
}

bool validate_object_schema(
    const ToolJsonValue::Object& input,
    const ToolObjectSchema& schema,
    std::string& out_error) {

    for (const auto& field : schema.fields) {
        const auto it = input.find(field.key);
        if (it == input.end()) {
            if (field.required) {
                out_error = "schema_required_key_missing: " + field.key;
                return false;
            }
            continue;
        }

        if (!value_matches_type(it->second, field.type)) {
            out_error = "schema_type_mismatch: key=" + field.key +
                ", expected=" + tool_json_type_to_string(field.type);
            return false;
        }
    }

    if (!schema.allow_additional_keys) {
        for (const auto& pair : input) {
            bool known = false;
            for (const auto& field : schema.fields) {
                if (field.key == pair.first) {
                    known = true;
                    break;
                }
            }

            if (!known) {
                out_error = "schema_unexpected_key: " + pair.first;
                return false;
            }
        }
    }

    return true;
}

bool is_all_whitespace(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
}

bool extract_tag_block_strict(
    const std::string& text,
    const std::string& open_tag,
    const std::string& close_tag,
    std::string& out_block) {

    const size_t open = text.find(open_tag);
    if (open == std::string::npos) {
        return false;
    }

    if (!is_all_whitespace(text.substr(0, open))) {
        return false;
    }

    const size_t content_start = open + open_tag.size();
    const size_t close = text.find(close_tag, content_start);
    if (close == std::string::npos) {
        return false;
    }

    const size_t after_close = close + close_tag.size();
    if (!is_all_whitespace(text.substr(after_close))) {
        return false;
    }

    if (text.find(open_tag, content_start) != std::string::npos) {
        return false;
    }
    if (text.find(close_tag, after_close) != std::string::npos) {
        return false;
    }

    out_block = text.substr(content_start, close - content_start);
    return true;
}

bool extract_tag_block_relaxed(
    const std::string& text,
    const std::string& open_tag,
    const std::string& close_tag,
    std::string& out_block) {

    const size_t open = text.find(open_tag);
    if (open == std::string::npos) {
        return false;
    }

    const size_t content_start = open + open_tag.size();
    const size_t close = text.find(close_tag, content_start);
    if (close == std::string::npos) {
        return false;
    }

    out_block = text.substr(content_start, close - content_start);
    return true;
}

// Helper: Strip Markdown code blocks (```) from text before strict parsing
static std::string strip_markdown_code_blocks(const std::string& text) {
    std::string result = text;

    // Remove leading ```
    size_t start = 0;
    while (start < result.size() && std::isspace(static_cast<unsigned char>(result[start]))) {
        ++start;
    }
    if (result.compare(start, 3, "```") == 0) {
        start += 3;
        // Skip optional language identifier (e.g., ```xml or ```json)
        while (start < result.size() && result[start] != '\n') {
            ++start;
        }
        if (start < result.size() && result[start] == '\n') {
            ++start;
        }
        result = result.substr(start);
    }

    // Remove trailing ```
    size_t end = result.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(result[end - 1]))) {
        --end;
    }
    if (end >= 3 && result.compare(end - 3, 3, "```") == 0) {
        end -= 3;
        // Strip trailing whitespace before code block marker
        while (end > 0 && std::isspace(static_cast<unsigned char>(result[end - 1]))) {
            --end;
        }
        result = result.substr(0, end);
    }

    return result;
}

}

void ToolRegistryExecutor::register_tool(const ToolSpec& spec, ToolHandler handler) {
    if (spec.name.empty() || !handler) {
        return;
    }

    tools_[spec.name] = RegisteredTool{ spec, std::nullopt, std::move(handler) };
}

void ToolRegistryExecutor::register_tool(
    const ToolSpec& spec,
    const ToolObjectSchema& schema,
    ToolHandler handler) {

    if (spec.name.empty() || !handler) {
        return;
    }

    ToolSpec normalized_spec = spec;
    if (normalized_spec.input_schema.empty()) {
        normalized_spec.input_schema = describe_object_schema(schema);
    }

    tools_[spec.name] = RegisteredTool{ normalized_spec, schema, std::move(handler) };
}

bool ToolRegistryExecutor::has_tool(const std::string& tool_name) const {
    return tools_.find(tool_name) != tools_.end();
}

std::vector<ToolSpec> ToolRegistryExecutor::list_tools() const {
    std::vector<ToolSpec> result;
    result.reserve(tools_.size());
    for (const auto& pair : tools_) {
        result.push_back(pair.second.spec);
    }

    std::sort(result.begin(), result.end(), [](const ToolSpec& a, const ToolSpec& b) {
        return a.name < b.name;
    });

    return result;
}

ToolResult ToolRegistryExecutor::execute(const ToolCall& call) {
    const auto it = tools_.find(call.name);
    if (it == tools_.end()) {
        ToolResult missing;
        missing.success = false;
        missing.error = "unknown_tool: " + call.name;
        return missing;
    }

    if (it->second.schema.has_value()) {
        ToolJsonInput json;
        if (!json.parse(call.input)) {
            ToolResult invalid;
            invalid.success = false;
            invalid.error = "invalid_tool_input_json: " + json.error_message();
            return invalid;
        }

        const ToolJsonValue::Object* obj = json.root().as_object();
        if (!obj) {
            ToolResult invalid;
            invalid.success = false;
            invalid.error = "schema_root_must_be_object";
            return invalid;
        }

        std::string schema_error;
        if (!validate_object_schema(*obj, *(it->second.schema), schema_error)) {
            ToolResult invalid;
            invalid.success = false;
            invalid.error = schema_error;
            return invalid;
        }
    }

    try {
        return it->second.handler(call.input);
    }
    catch (const std::exception& ex) {
        ToolResult failed;
        failed.success = false;
        failed.error = std::string("tool_exception: ") + ex.what();
        return failed;
    }
    catch (...) {
        ToolResult failed;
        failed.success = false;
        failed.error = "tool_exception: unknown";
        return failed;
    }
}

std::string ToolIOProtocol::build_tool_guide(const std::vector<ToolSpec>& tools) {
    std::ostringstream oss;

    if (tools.empty()) {
        oss << "ツールは有効化されていますが、利用可能なツールは登録されていません。";
        return oss.str();
    }

    oss << "必要に応じてツールを呼び出してください。\n";
    oss << "使い方の詳細は help ツールを呼び出して確認してください。\n\n";
    oss << "利用可能ツール一覧（name / description）:\n";

    for (const auto& tool : tools) {
        oss << "- " << tool.name << ": " << tool.description << "\n";
    }

    oss << "\n例: help に {\"tool\":\"sum_numbers\"} を渡すと使い方を取得できます。\n";
    return oss.str();
}

std::string ToolIOProtocol::build_output_contract_guide(bool allow_tool_call) {
    std::ostringstream oss;

    oss << "出力契約（現行2フェーズ運用）:\n";
    if (allow_tool_call) {
        oss << "- Tool Phase: <tool_call> ブロックのみを出力。\n";
        oss << "  形式:\n";
        oss << "  <tool_call>\n";
        oss << "  name: <tool_name>\n";
        oss << "  input:\n";
        oss << "  <tool_input_text_or_json>\n";
        oss << "  </tool_call>\n";
    }
    else {
        oss << "- Tool Phase は無効（このターンではツール呼び出しなし）。\n";
    }

    oss << "- Response Phase: <assistant_response> を推奨。\n";
    oss << "  形式:\n";
    oss << "  <assistant_response>\n";
    oss << "  <ユーザーに返す自然な応答本文>\n";
    oss << "  </assistant_response>\n";
    oss << "- 禁止: 見出し・分析メモ・契約文の再掲・不要な前置き。\n";
    return oss.str();
}

std::string ToolIOProtocol::build_tool_call_contract_guide() {
    std::ostringstream oss;
    oss << "【Tool Phase 出力契約 / 厳守】\n\n";
    oss << "必須: <tool_call> ブロック1つのみ。前置き・説明・コメント不要。\n\n";
    oss << "形式:\n";
    oss << "<tool_call>\n";
    oss << "name: <ツール名>\n";
    oss << "input: <JSON形式の入力>\n";
    oss << "</tool_call>\n\n";
    oss << "例1（JSON入力）:\n";
    oss << "<tool_call>\n";
    oss << "name: number_guess_game\n";
    oss << "input: { \"action\": \"start\", \"min\": 1, \"max\": 100 }\n";
    oss << "</tool_call>\n\n";
    oss << "例2（シンプル入力）:\n";
    oss << "<tool_call>\n";
    oss << "name: get_current_time\n";
    oss << "input: {}\n";
    oss << "</tool_call>\n\n";
    oss << "例3（ツール終了）:\n";
    oss << "<tool_call>\n";
    oss << "name: finish_tool_planning\n";
    oss << "input: {}\n";
    oss << "</tool_call>\n\n";
    oss << "禁止事項:\n";
    oss << "- Markdownコードブロック（```）で囲まない\n";
    oss << "- 「それでは」「まず」などの前置き不要\n";
    oss << "- tool_callブロックの外に説明文を書かない\n";
    oss << "- 複数のtool_callブロックを同時出力しない\n";
    return oss.str();
}

std::string ToolIOProtocol::build_assistant_contract_guide() {
    std::ostringstream oss;
    oss << "出力契約（Response Phase）:\n";
    oss << "- 推奨形式は assistant_response ブロック1つのみ。\n";
    oss << "- 追加の見出し・分析メモ・注意書きは出力しない。\n\n";
    oss << "<assistant_response>\n";
    oss << "<ユーザーに返す自然な応答本文>\n";
    oss << "</assistant_response>\n";
    return oss.str();
}

bool ToolIOProtocol::try_parse_assistant_response(const std::string& llm_output, std::string& out_response) {
    const std::string assistant_channel_open = "<assistant_channel>";
    const std::string assistant_channel_close = "</assistant_channel>";
    const std::string open_tag = "<assistant_response>";
    const std::string close_tag = "</assistant_response>";

    std::string channel_block;
    if (extract_tag_block_relaxed(llm_output, assistant_channel_open, assistant_channel_close, channel_block)) {
        std::string block;
        if (!extract_tag_block_relaxed(channel_block, open_tag, close_tag, block)) {
            return false;
        }

        out_response = trim_copy(block);
        return !out_response.empty();
    }

    std::string block;
    if (!extract_tag_block_relaxed(llm_output, open_tag, close_tag, block)) {
        return false;
    }

    out_response = trim_copy(block);
    return !out_response.empty();
}

bool ToolIOProtocol::try_parse_tool_call(const std::string& llm_output, ToolCall& out_call) {
    const std::string tool_channel_open = "<tool_channel>";
    const std::string tool_channel_close = "</tool_channel>";
    const std::string open_tag = "<tool_call>";
    const std::string close_tag = "</tool_call>";

    std::string channel_block;
    if (extract_tag_block_relaxed(llm_output, tool_channel_open, tool_channel_close, channel_block)) {
        std::string block;
        if (!extract_tag_block_relaxed(channel_block, open_tag, close_tag, block)) {
            return false;
        }

        std::istringstream iss(block);
        std::string line;

        std::string tool_name;
        std::ostringstream input_builder;
        bool reading_input = false;

        while (std::getline(iss, line)) {
            const std::string trimmed = trim_copy(line);

            if (!reading_input && trimmed.rfind("name:", 0) == 0) {
                tool_name = trim_copy(trimmed.substr(5));
                continue;
            }

            if (!reading_input && trimmed.rfind("input:", 0) == 0) {
                reading_input = true;
                std::string same_line_input = trim_copy(trimmed.substr(6));
                if (!same_line_input.empty()) {
                    input_builder << same_line_input;
                }
                continue;
            }

            if (reading_input) {
                if (input_builder.tellp() > 0) {
                    input_builder << "\n";
                }
                input_builder << line;
            }
        }

        tool_name = trim_copy(tool_name);
        if (tool_name.empty()) {
            return false;
        }

        out_call.name = tool_name;
        out_call.input = trim_copy(input_builder.str());
        return true;
    }

    std::string block;
    if (!extract_tag_block_relaxed(llm_output, open_tag, close_tag, block)) {
        return false;
    }

    std::istringstream iss(block);
    std::string line;

    std::string tool_name;
    std::ostringstream input_builder;
    bool reading_input = false;

    while (std::getline(iss, line)) {
        const std::string trimmed = trim_copy(line);

        if (!reading_input && trimmed.rfind("name:", 0) == 0) {
            tool_name = trim_copy(trimmed.substr(5));
            continue;
        }

        if (!reading_input && trimmed.rfind("input:", 0) == 0) {
            reading_input = true;
            std::string same_line_input = trim_copy(trimmed.substr(6));
            if (!same_line_input.empty()) {
                input_builder << same_line_input;
            }
            continue;
        }

        if (reading_input) {
            if (input_builder.tellp() > 0) {
                input_builder << "\n";
            }
            input_builder << line;
        }
    }

    tool_name = trim_copy(tool_name);
    if (tool_name.empty()) {
        return false;
    }

    out_call.name = tool_name;
    out_call.input = trim_copy(input_builder.str());
    return true;
}

bool ToolIOProtocol::try_parse_assistant_response_strict(const std::string& llm_output, std::string& out_response) {
    const std::string open_tag = "<assistant_response>";
    const std::string close_tag = "</assistant_response>";

    // Relaxed extraction: ignore text before/after tags
    std::string block;
    if (!extract_tag_block_relaxed(llm_output, open_tag, close_tag, block)) {
        return false;
    }

    out_response = trim_copy(block);
    return !out_response.empty();
}

bool ToolIOProtocol::try_parse_tool_call_strict(const std::string& llm_output, ToolCall& out_call) {
    const std::string open_tag = "<tool_call>";
    const std::string close_tag = "</tool_call>";

    // Relaxed extraction: ignore text before/after tags (e.g., ```, <end_of_turn>)
    std::string block;
    if (!extract_tag_block_relaxed(llm_output, open_tag, close_tag, block)) {
        return false;
    }

    std::istringstream iss(block);
    std::string line;

    std::string tool_name;
    std::ostringstream input_builder;
    bool reading_input = false;

    while (std::getline(iss, line)) {
        const std::string trimmed = trim_copy(line);

        if (!reading_input && trimmed.rfind("name:", 0) == 0) {
            tool_name = trim_copy(trimmed.substr(5));
            continue;
        }

        if (!reading_input && trimmed.rfind("input:", 0) == 0) {
            reading_input = true;
            std::string same_line_input = trim_copy(trimmed.substr(6));
            if (!same_line_input.empty()) {
                input_builder << same_line_input;
            }
            continue;
        }

        if (reading_input) {
            if (input_builder.tellp() > 0) {
                input_builder << "\n";
            }
            input_builder << line;
        }
    }

    tool_name = trim_copy(tool_name);
    if (tool_name.empty()) {
        return false;
    }

    out_call.name = tool_name;
    out_call.input = trim_copy(input_builder.str());
    return true;
}

std::string ToolIOProtocol::build_tool_result_block(const ToolCall& call, const ToolResult& result) {
    std::ostringstream oss;
    oss << "<tool_result>\n";
    oss << "name: " << call.name << "\n";
    oss << "success: " << (result.success ? "true" : "false") << "\n";

    if (result.success) {
        oss << "output:\n";
        oss << result.output << "\n";
    }
    else {
        oss << "error:\n";
        oss << result.error << "\n";
    }

    oss << "</tool_result>";
    return oss.str();
}

std::string ToolIOProtocol::trim_copy(const std::string& value) {
    auto is_ws = [](unsigned char c) {
        return std::isspace(c) != 0;
    };

    auto first = std::find_if_not(value.begin(), value.end(), is_ws);
    if (first == value.end()) {
        return "";
    }

    auto last = std::find_if_not(value.rbegin(), value.rend(), is_ws).base();
    return std::string(first, last);
}
