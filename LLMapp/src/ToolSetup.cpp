#include "ToolSetup.h"

#include <cmath>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <limits>
#include <optional>
#include <random>
#include <sstream>

namespace {

struct NumberGuessGameState {
    bool active = false;
    int minimum = 1;
    int maximum = 100;
    int secret = 0;
    int attempts_remaining = 0;
    int max_attempts = 0;
    bool won = false;
};

NumberGuessGameState g_number_guess_game;

bool try_get_int_field(
    const ToolJsonValue::Object& obj,
    const std::string& key,
    int& out_value,
    std::string& out_error,
    bool required,
    int min_value = std::numeric_limits<int>::min(),
    int max_value = std::numeric_limits<int>::max()) {

    const auto it = obj.find(key);
    if (it == obj.end()) {
        if (required) {
            out_error = "missing_required_field: " + key;
            return false;
        }
        return true;
    }

    const auto number = it->second.as_number();
    if (!number) {
        out_error = "field_must_be_number: " + key;
        return false;
    }

    const double raw = *number;
    if (!std::isfinite(raw) || std::floor(raw) != raw) {
        out_error = "field_must_be_integer: " + key;
        return false;
    }

    if (raw < static_cast<double>(min_value) || raw > static_cast<double>(max_value)) {
        out_error = "field_out_of_range: " + key;
        return false;
    }

    out_value = static_cast<int>(raw);
    return true;
}

bool try_get_string_field(
    const ToolJsonValue::Object& obj,
    const std::string& key,
    std::string& out_value,
    std::string& out_error,
    bool required) {

    const auto it = obj.find(key);
    if (it == obj.end()) {
        if (required) {
            out_error = "missing_required_field: " + key;
            return false;
        }
        return true;
    }

    const auto value = it->second.as_string();
    if (!value) {
        out_error = "field_must_be_string: " + key;
        return false;
    }

    out_value = *value;
    return true;
}

std::string build_game_status_json(const NumberGuessGameState& state, const std::string& status) {
    std::ostringstream oss;
    oss << "{"
        << "\"status\":\"" << status << "\"," 
        << "\"active\":" << (state.active ? "true" : "false") << ","
        << "\"won\":" << (state.won ? "true" : "false") << ","
        << "\"min\":" << state.minimum << ","
        << "\"max\":" << state.maximum << ","
        << "\"attempts_remaining\":" << state.attempts_remaining << ","
        << "\"max_attempts\":" << state.max_attempts
        << "}";
    return oss.str();
}

}

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

    agent.register_tool(
        { "number_guess_game", "数当てゲームを操作します（start/guess/status/reset）", "" },
        ToolObjectSchema{
            {
                ToolFieldSchema{ "action", ToolJsonType::STRING, true },
                ToolFieldSchema{ "guess", ToolJsonType::NUMBER, false },
                ToolFieldSchema{ "min", ToolJsonType::NUMBER, false },
                ToolFieldSchema{ "max", ToolJsonType::NUMBER, false },
                ToolFieldSchema{ "max_attempts", ToolJsonType::NUMBER, false },
                ToolFieldSchema{ "secret", ToolJsonType::NUMBER, false },
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

            std::string parse_error;
            std::string action;
            if (!try_get_string_field(*obj, "action", action, parse_error, true)) {
                result.success = false;
                result.error = parse_error;
                return result;
            }

            if (action == "start") {
                int minimum = 1;
                int maximum = 100;
                int max_attempts = 10;
                int secret = 0;

                if (!try_get_int_field(*obj, "min", minimum, parse_error, false) ||
                    !try_get_int_field(*obj, "max", maximum, parse_error, false) ||
                    !try_get_int_field(*obj, "max_attempts", max_attempts, parse_error, false, 1)) {
                    result.success = false;
                    result.error = parse_error;
                    return result;
                }

                if (minimum >= maximum) {
                    result.success = false;
                    result.error = "invalid_range: min_must_be_less_than_max";
                    return result;
                }

                const bool has_secret = obj->find("secret") != obj->end();
                if (has_secret) {
                    if (!try_get_int_field(*obj, "secret", secret, parse_error, true, minimum, maximum)) {
                        result.success = false;
                        result.error = parse_error;
                        return result;
                    }
                }
                else {
                    std::random_device rd;
                    std::mt19937 engine(rd());
                    std::uniform_int_distribution<int> dist(minimum, maximum);
                    secret = dist(engine);
                }

                g_number_guess_game.active = true;
                g_number_guess_game.minimum = minimum;
                g_number_guess_game.maximum = maximum;
                g_number_guess_game.secret = secret;
                g_number_guess_game.attempts_remaining = max_attempts;
                g_number_guess_game.max_attempts = max_attempts;
                g_number_guess_game.won = false;

                result.success = true;
                result.output = build_game_status_json(g_number_guess_game, "started");
                return result;
            }

            if (action == "status") {
                result.success = true;
                if (!g_number_guess_game.active) {
                    NumberGuessGameState idle;
                    result.output = build_game_status_json(idle, "idle");
                    return result;
                }
                result.output = build_game_status_json(g_number_guess_game, "in_progress");
                return result;
            }

            if (action == "reset") {
                g_number_guess_game = NumberGuessGameState{};
                result.success = true;
                result.output = build_game_status_json(g_number_guess_game, "reset");
                return result;
            }

            if (action == "guess") {
                if (!g_number_guess_game.active) {
                    result.success = false;
                    result.error = "game_not_started";
                    return result;
                }

                int guess = 0;
                if (!try_get_int_field(*obj, "guess", guess, parse_error, true,
                    g_number_guess_game.minimum, g_number_guess_game.maximum)) {
                    result.success = false;
                    result.error = parse_error;
                    return result;
                }

                g_number_guess_game.attempts_remaining -= 1;

                std::ostringstream oss;
                oss << "{";
                oss << "\"status\":\"guess_result\",";
                oss << "\"guess\":" << guess << ",";

                if (guess == g_number_guess_game.secret) {
                    g_number_guess_game.active = false;
                    g_number_guess_game.won = true;
                    oss << "\"result\":\"correct\",";
                    oss << "\"finished\":true,";
                    oss << "\"won\":true,";
                    oss << "\"attempts_remaining\":" << g_number_guess_game.attempts_remaining;
                }
                else if (g_number_guess_game.attempts_remaining <= 0) {
                    g_number_guess_game.active = false;
                    g_number_guess_game.won = false;
                    oss << "\"result\":\"game_over\",";
                    oss << "\"hint\":\"" << (guess < g_number_guess_game.secret ? "higher" : "lower") << "\",";
                    oss << "\"finished\":true,";
                    oss << "\"won\":false,";
                    oss << "\"answer\":" << g_number_guess_game.secret << ",";
                    oss << "\"attempts_remaining\":0";
                }
                else {
                    oss << "\"result\":\"continue\",";
                    oss << "\"hint\":\"" << (guess < g_number_guess_game.secret ? "higher" : "lower") << "\",";
                    oss << "\"finished\":false,";
                    oss << "\"won\":false,";
                    oss << "\"attempts_remaining\":" << g_number_guess_game.attempts_remaining;
                }

                oss << "}";

                result.success = true;
                result.output = oss.str();
                return result;
            }

            result.success = false;
            result.error = "unknown_action: " + action;
            return result;
        }
    );
}
