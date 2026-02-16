#include "ToolSetup.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>

void register_default_tools(EmotionalAgent& agent) {
    // ===== ツールI/Oインターフェースの標準登録 =====
    // 入力は文字列（JSON可）として受け取り、出力/エラーを返す。

    agent.register_tool(
        { "get_current_time", "現在のローカル時刻を取得します", "入力不要（空文字で可）" },
        [](const std::string&) {
            ToolResult result;

            auto now = std::chrono::system_clock::now();
            const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf{};
            localtime_s(&tm_buf, &now_time);

            std::ostringstream oss;
            oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");

            result.success = true;
            result.output = oss.str();
            return result;
        }
    );

    agent.register_tool(
        { "echo", "受け取った入力文字列をそのまま返します", "任意の文字列" },
        [](const std::string& input) {
            ToolResult result;
            result.success = true;
            result.output = input;
            return result;
        }
    );

    // JSON入力を厳密パースする例
    // 入力例: {"a": 10, "b": 20}
    agent.register_tool(
        { "sum_numbers", "JSONで渡されたa,bを加算します", "" },
        ToolObjectSchema{
            {
                ToolFieldSchema{ "a", ToolJsonType::NUMBER, true },
                ToolFieldSchema{ "b", ToolJsonType::NUMBER, true },
            },
            false
        },
        [](const std::string& input) {
            ToolResult result;

            ToolJsonInput json;
            if (!json.parse(input)) {
                result.success = false;
                result.error = "invalid_json: " + json.error_message();
                return result;
            }

            const ToolJsonValue::Object* obj = json.root().as_object();
            if (!obj) {
                result.success = false;
                result.error = "input_must_be_json_object";
                return result;
            }

            const auto a_it = obj->find("a");
            const auto b_it = obj->find("b");
            const auto a = (a_it != obj->end()) ? a_it->second.as_number() : std::nullopt;
            const auto b = (b_it != obj->end()) ? b_it->second.as_number() : std::nullopt;
            if (!a || !b) {
                result.success = false;
                result.error = "schema_validated_but_value_read_failed";
                return result;
            }

            std::ostringstream oss;
            oss << (*a + *b);

            result.success = true;
            result.output = oss.str();
            return result;
        }
    );
}
