#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

/**
 * @brief LLMが利用可能なツール定義
 */
struct ToolSpec {
    std::string name;          // 例: "get_time"
    std::string description;   // 例: "現在時刻を取得する"
    std::string input_schema;  // 例: "JSON文字列。{\"timezone\":\"Asia/Tokyo\"}"
};

/**
 * @brief LLMが要求したツール呼び出し
 */
struct ToolCall {
    std::string name;
    std::string input;
};

/**
 * @brief ツール実行結果
 */
struct ToolResult {
    bool success = false;
    std::string output;
    std::string error;
};

/**
 * @brief ツール入力JSON値
 */
struct ToolJsonValue {
    using Object = std::unordered_map<std::string, ToolJsonValue>;
    using Array = std::vector<ToolJsonValue>;
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Object, Array>;

    Storage value = nullptr;

    bool is_null() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_string() const;
    bool is_object() const;
    bool is_array() const;

    std::optional<bool> as_bool() const;
    std::optional<double> as_number() const;
    std::optional<std::string> as_string() const;
    const Object* as_object() const;
    const Array* as_array() const;
};

/**
 * @brief ツール入力JSON（厳密パース）
 *
 * - 入力が空の場合は parse() で失敗
 * - JSON文法違反時は parse() が false を返し、error_message() で詳細確認
 * - 文字列以外の任意JSON（object/array/number/bool/null）に対応
 *
 * 使い方:
 * @code
 * ToolJsonInput json;
 * if (!json.parse(input)) {
 *     ToolResult r;
 *     r.success = false;
 *     r.error = json.error_message();
 *     return r;
 * }
 *
 * const ToolJsonValue::Object* obj = json.root().as_object();
 * if (!obj) { ... }
 * @endcode
 */
class ToolJsonInput {
public:
    bool parse(const std::string& text);
    const ToolJsonValue& root() const { return root_; }
    const std::string& error_message() const { return error_message_; }

private:
    ToolJsonValue root_;
    std::string error_message_;
};

/**
 * @brief JSONの型種別（スキーマ宣言用）
 */
enum class ToolJsonType {
    ANY,
    NULL_VALUE,
    BOOL,
    NUMBER,
    STRING,
    OBJECT,
    ARRAY,
};

/**
 * @brief オブジェクト入力の1フィールド制約
 */
struct ToolFieldSchema {
    std::string key;
    ToolJsonType type = ToolJsonType::ANY;
    bool required = true;
};

/**
 * @brief オブジェクト入力の簡易スキーマ
 */
struct ToolObjectSchema {
    std::vector<ToolFieldSchema> fields;
    bool allow_additional_keys = true;
};

/**
 * @brief ツール実行の抽象インターフェース
 */
class IToolExecutor {
public:
    virtual ~IToolExecutor() = default;

    /**
     * @brief 利用可能ツール一覧を返す
     */
    virtual std::vector<ToolSpec> list_tools() const = 0;

    /**
     * @brief ツールを実行する
     */
    virtual ToolResult execute(const ToolCall& call) = 0;
};

/**
 * @brief コールバック登録型の汎用ツール実行器
 *
 * 使い方:
 * @code
 * ToolRegistryExecutor registry;
 * registry.register_tool(
 *     {"get_time", "現在時刻を返す", "入力不要。空文字でOK"},
 *     [](const std::string&) {
 *         ToolResult r;
 *         r.success = true;
 *         r.output = "2026-02-16 10:00:00";
 *         return r;
 *     }
 * );
 *
 * EmotionalAgent agent(...);
 * agent.set_tool_executor(&registry);
 * @endcode
 */
class ToolRegistryExecutor : public IToolExecutor {
public:
    using ToolHandler = std::function<ToolResult(const std::string& input)>;

    void register_tool(const ToolSpec& spec, ToolHandler handler);

    /**
     * @brief スキーマ付きでツールを登録
     *
     * 実行時に以下を自動検証します:
     * - input が妥当なJSONであること
     * - JSONルートが object であること
     * - requiredフィールドが存在すること
     * - 各フィールドの型が一致すること
     */
    void register_tool(const ToolSpec& spec, const ToolObjectSchema& schema, ToolHandler handler);

    bool has_tool(const std::string& tool_name) const;

    std::vector<ToolSpec> list_tools() const override;
    ToolResult execute(const ToolCall& call) override;

private:
    struct RegisteredTool {
        ToolSpec spec;
        std::optional<ToolObjectSchema> schema;
        ToolHandler handler;
    };

    std::unordered_map<std::string, RegisteredTool> tools_;
};

/**
 * @brief LLMとのツール入出力プロトコル
 *
 * 現行運用（中間案）:
 * - Tool Phase: <tool_call> ブロックのみを厳格に許可
 * - Response Phase: <assistant_response> を推奨（自然文フォールバックは上位層で扱う）
 *
 * Tool Phase の出力フォーマット（厳守）:
 * @code
 * <tool_call>
 * name: get_time
 * input:
 * {"timezone":"Asia/Tokyo"}
 * </tool_call>
 *
 * Response Phase の推奨フォーマット:
 * @code
 * <assistant_response>
 * こんにちは。今日はどんなことを試しますか？
 * </assistant_response>
 * @endcode
 *
 * ツール実行結果をLLMへ返すフォーマット:
 * @code
 * <tool_result>
 * name: get_time
 * success: true
 * output:
 * 2026-02-16 10:00:00
 * </tool_result>
 * @endcode
 */
class ToolIOProtocol {
public:
    static std::string build_tool_guide(const std::vector<ToolSpec>& tools);
    // 互換用途: 旧呼び出し側向けの統合契約ガイド（内部は現行2フェーズ方針を説明）
    static std::string build_output_contract_guide(bool allow_tool_call);
    static std::string build_tool_call_contract_guide();
    static std::string build_assistant_contract_guide();
    static bool try_parse_assistant_response(const std::string& llm_output, std::string& out_response);
    static bool try_parse_tool_call(const std::string& llm_output, ToolCall& out_call);
    static bool try_parse_assistant_response_strict(const std::string& llm_output, std::string& out_response);
    static bool try_parse_tool_call_strict(const std::string& llm_output, ToolCall& out_call);
    static std::string build_tool_result_block(const ToolCall& call, const ToolResult& result);

private:
    static std::string trim_copy(const std::string& value);
};
