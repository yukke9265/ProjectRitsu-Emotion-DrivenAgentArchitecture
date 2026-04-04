#include "InputAnalyzer.h"
#include "LLMInference.h"
#include "MemoryController.h"
#include "Config.h"
#include <algorithm>
#include <array>
#include <sstream>
#include <cctype>
#include <regex>
#include <iostream>

namespace {
struct IntentMapping {
    Intent value;
    const char* name;
};

struct EvaluationMapping {
    EvaluationToAI value;
    const char* name;
};

constexpr std::array<IntentMapping, 5> kIntentMappings = {{
    {Intent::PRAISE, "praise"},
    {Intent::CRITICISM, "criticism"},
    {Intent::QUESTION, "question"},
    {Intent::GREETING, "greeting"},
    {Intent::CASUAL, "casual"},
}};

constexpr std::array<EvaluationMapping, 3> kEvaluationMappings = {{
    {EvaluationToAI::POSITIVE, "positive"},
    {EvaluationToAI::NEUTRAL, "neutral"},
    {EvaluationToAI::NEGATIVE, "negative"},
}};
}

std::string intent_to_string(Intent intent) {
    switch (intent) {
        case Intent::PRAISE: return "praise";
        case Intent::CRITICISM: return "criticism";
        case Intent::QUESTION: return "question";
        case Intent::GREETING: return "greeting";
        case Intent::CASUAL: return "casual";
        default: return "unknown";
    }
}

Intent intent_from_string(const std::string& intent) {
    for (const auto& mapping : kIntentMappings) {
        if (intent == mapping.name) {
            return mapping.value;
        }
    }
    return Intent::UNKNOWN;
}

std::string evaluation_to_string(EvaluationToAI evaluation) {
    switch (evaluation) {
        case EvaluationToAI::POSITIVE: return "positive";
        case EvaluationToAI::NEUTRAL: return "neutral";
        case EvaluationToAI::NEGATIVE: return "negative";
        default: return "unknown";
    }
}

EvaluationToAI evaluation_from_string(const std::string& evaluation) {
    for (const auto& mapping : kEvaluationMappings) {
        if (evaluation == mapping.name) {
            return mapping.value;
        }
    }
    return EvaluationToAI::UNKNOWN;
}

namespace {
bool is_valid_intent(Intent intent) {
    return intent != Intent::UNKNOWN;
}

bool is_valid_evaluation(EvaluationToAI evaluation) {
    return evaluation != EvaluationToAI::UNKNOWN;
}

bool is_semantically_valid_llm_result(const AnalyzedInput& result) {
    if (result.topic.empty()) return false;
    if (!is_valid_intent(result.intent)) return false;
    if (!is_valid_evaluation(result.evaluation_to_ai)) return false;
    if (result.sentiment_score < INPUT_ANALYZER_SENTIMENT_MIN ||
        result.sentiment_score > INPUT_ANALYZER_SENTIMENT_MAX) return false;
    return true;
}

void log_unknown_analysis_labels(const AnalyzedInput& result, const char* source) {
#if !INPUT_ANALYZER_LOG_UNKNOWN_LABELS
    (void)result;
    (void)source;
    return;
#endif

    const bool intent_unknown = (result.intent == Intent::UNKNOWN);
    const bool evaluation_unknown = (result.evaluation_to_ai == EvaluationToAI::UNKNOWN);

    if (!intent_unknown && !evaluation_unknown) {
        return;
    }

    std::cerr << "[警告] " << source << " で未知ラベルを検出: ";
    if (intent_unknown) {
        std::cerr << "intent=UNKNOWN";
    }
    if (intent_unknown && evaluation_unknown) {
        std::cerr << ", ";
    }
    if (evaluation_unknown) {
        std::cerr << "evaluation_to_ai=UNKNOWN";
    }
    std::cerr << " | input=\"" << result.raw_text << "\"" << std::endl;
}

std::string trim_copy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])) != 0) {
        ++start;
    }

    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }

    return s.substr(start, end - start);
}

std::string normalize_analysis_text(const std::string& text) {
    std::string out;
    out.reserve(text.size());

    bool prev_space = false;
    bool prev_newline = false;

    for (char ch : text) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (ch == '\r') {
            continue;
        }

        if (ch == '\n') {
            if (!prev_newline) {
                out.push_back('\n');
                prev_newline = true;
            }
            prev_space = false;
            continue;
        }

        if (std::isspace(uch) != 0) {
            if (!prev_space) {
                out.push_back(' ');
                prev_space = true;
            }
            prev_newline = false;
            continue;
        }

        // 制御文字は除去する（改行は上で処理済み）
        if (std::iscntrl(uch) != 0) {
            continue;
        }

        out.push_back(ch);
        prev_space = false;
        prev_newline = false;
    }

    return trim_copy(out);
}

std::string extract_first_json_object(const std::string& text) {
    bool in_string = false;
    bool escape = false;
    int depth = 0;
    size_t start = std::string::npos;

    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];

        if (escape) {
            escape = false;
            continue;
        }

        if (ch == '\\') {
            if (in_string) {
                escape = true;
            }
            continue;
        }

        if (ch == '"') {
            in_string = !in_string;
            continue;
        }

        if (in_string) {
            continue;
        }

        if (ch == '{') {
            if (depth == 0) {
                start = i;
            }
            ++depth;
            continue;
        }

        if (ch == '}') {
            if (depth == 0) {
                continue;
            }
            --depth;
            if (depth == 0 && start != std::string::npos) {
                return text.substr(start, i - start + 1);
            }
        }
    }

    return "";
}

std::string unescape_json_string(std::string s) {
    s = std::regex_replace(s, std::regex(R"(\\n)"), "\n");
    s = std::regex_replace(s, std::regex(R"(\\r)"), "\r");
    s = std::regex_replace(s, std::regex(R"(\\t)"), "\t");
    s = std::regex_replace(s, std::regex(R"(\\\")"), "\"");
    s = std::regex_replace(s, std::regex(R"(\\\\)"), "\\");
    return s;
}

std::string find_missing_field_message(const AnalyzedInput& result) {
    std::vector<std::string> missing;
    if (result.topic.empty()) {
        missing.push_back("topic");
    }
    if (result.intent == Intent::UNKNOWN) {
        missing.push_back("intent");
    }
    if (result.evaluation_to_ai == EvaluationToAI::UNKNOWN) {
        missing.push_back("evaluation_to_ai");
    }
    if (result.keywords.empty()) {
        missing.push_back("keywords");
    }
    if (result.sentiment_score < INPUT_ANALYZER_SENTIMENT_MIN ||
        result.sentiment_score > INPUT_ANALYZER_SENTIMENT_MAX) {
        missing.push_back("sentiment_score");
    }

    if (missing.empty()) {
        return "";
    }

    std::ostringstream oss;
    oss << "必須項目不足/不正: ";
    for (size_t i = 0; i < missing.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << missing[i];
    }
    return oss.str();
}
}

InputAnalyzer::InputAnalyzer() 
    : llm_inference_(nullptr)
    , use_llm_(false)
    , debug_mode_(false) {
    initialize_dictionaries();
}

InputAnalyzer::~InputAnalyzer() {
}

void InputAnalyzer::set_llm_inference(LLMInference* llm) {
    llm_inference_ = llm;
}

void InputAnalyzer::enable_llm_mode(bool enable) {
    use_llm_ = enable;
    if (enable && llm_inference_ == nullptr) {
        std::cerr << "[警告] LLMモードが有効ですが、LLMインスタンスが設定されていません。" << std::endl;
    }
}

void InputAnalyzer::initialize_dictionaries() {
    // ポジティブキーワード
    positive_keywords_ = { INPUT_ANALYZER_POSITIVE_KEYWORDS };

    // ネガティブキーワード
    negative_keywords_ = { INPUT_ANALYZER_NEGATIVE_KEYWORDS };

    // 賞賛キーワード
    praise_keywords_ = { INPUT_ANALYZER_PRAISE_KEYWORDS };

    // 批判キーワード
    criticism_keywords_ = { INPUT_ANALYZER_CRITICISM_KEYWORDS };
}

AnalyzedInput InputAnalyzer::analyze(const std::string& user_input,
                                     const std::deque<ConversationTurn>* recent_history) {
    const std::string normalized_input = normalize_analysis_text(user_input);

    // ハイブリッドモード：LLMとキーワードベースの両方を試行
    if (use_llm_ && llm_inference_ != nullptr) {
        try {
            // LLMベースの分析を試行
            AnalyzedInput llm_result = analyze_with_llm(normalized_input, recent_history);
            
            // LLM結果の妥当性チェック（基本的な検証）
            if (llm_result.intent != Intent::UNKNOWN && 
                llm_result.sentiment_score >= INPUT_ANALYZER_SENTIMENT_MIN && 
                llm_result.sentiment_score <= INPUT_ANALYZER_SENTIMENT_MAX) {
                // LLM結果が妥当なので採用
                return llm_result;
            }
            
            // LLM結果が不完全な場合はキーワードベースにフォールバック
            std::cerr << "[情報] LLM分析結果が不完全なため、キーワードベースにフォールバック" << std::endl;
        } catch (const std::exception& e) {
            // LLM処理でエラーが発生した場合もフォールバック
            std::cerr << "[警告] LLM分析中にエラー: " << e.what() << std::endl;
        }
    }
    
    // キーワードベースの分析（フォールバック or デフォルト）
    return analyze_with_keywords(normalized_input);
}

AnalyzedInput InputAnalyzer::analyze_with_keywords(const std::string& user_input) {
    AnalyzedInput result;
    result.raw_text = user_input;

    // 各種分析を実行（従来の方式）
    result.topic = extract_topic(user_input);
    result.intent = classify_intent(user_input);
    result.evaluation_to_ai = evaluate_ai_attitude(user_input);
    result.keywords = extract_keywords(user_input);
    result.sentiment_score = calculate_sentiment(user_input);

    log_unknown_analysis_labels(result, "キーワード分析");

    return result;
}

AnalyzedInput InputAnalyzer::analyze_with_llm(const std::string& user_input,
                                              const std::deque<ConversationTurn>* recent_history) {
    // プロンプトを生成
    std::string prompt = create_analysis_prompt(user_input, recent_history);
    
    if (debug_mode_) {
        std::cout << "  [InputAnalyzer] LLMへのプロンプト:\n";
        std::cout << "  " << std::string(50, '-') << "\n";
        // プロンプトが長い場合は最初と最後だけ表示
        if (prompt.length() > INPUT_ANALYZER_PROMPT_PREVIEW_MAX_LENGTH) {
            std::cout << "  " << prompt.substr(0, INPUT_ANALYZER_PROMPT_PREVIEW_HEAD_LENGTH) << "\n";
            std::cout << "  [...省略 "
                      << (prompt.length() - INPUT_ANALYZER_PROMPT_PREVIEW_MAX_LENGTH)
                      << " 文字...]\n";
            std::cout << "  " << prompt.substr(prompt.length() - INPUT_ANALYZER_PROMPT_PREVIEW_TAIL_LENGTH) << "\n";
        } else {
            std::cout << "  " << prompt << "\n";
        }
        std::cout << "  " << std::string(50, '-') << "\n\n";
    }
    
    // パース成功 + 意味的妥当性チェック成功まで最大LLM_PARSE_RETRY_COUNT回リトライ
    for (int retry = 0; retry < LLM_PARSE_RETRY_COUNT; retry++) {
        try {
            if (debug_mode_ && retry > 0) {
                std::cout << "  [InputAnalyzer] リトライ " << retry + 1 << "回目\n";
            }

            std::string retry_prompt = prompt;
            if (retry > 0) {
                retry_prompt += "\n\n【重要】前回の出力は不完全でした。";
                retry_prompt += "topic/intent/evaluation_to_ai/sentiment_score を必ず有効値で埋め、";
                retry_prompt += "JSONオブジェクトのみを1つ返してください。";
                retry_prompt += "\n前回エラー: ";
                retry_prompt += e.what();
            }
            
            // LLMで単発推論（KVキャッシュは自動クリア）
            std::string llm_output = llm_inference_->infer_stateless(retry_prompt);
            
            if (debug_mode_) {
                std::cout << "  [InputAnalyzer] LLM応答:\n";
                std::cout << "  " << std::string(50, '-') << "\n";
                std::cout << "  " << llm_output << "\n";
                std::cout << "  " << std::string(50, '-') << "\n\n";
            }
            
            // LLM応答をパースして構造化データに変換
            AnalyzedInput result = parse_llm_response(llm_output, user_input);

            log_unknown_analysis_labels(result, "LLM分析");

            // JSONとしては解釈できても、空項目や未知ラベルは再生成対象にする
            if (!is_semantically_valid_llm_result(result)) {
                throw std::runtime_error("LLM出力はJSON形式だが内容が不完全/不正です");
            }
            
            // パース成功 - 結果を返す
            if (retry > 0) {
                std::cout << "[情報] LLMパース成功（リトライ " << retry + 1 << "回目）" << std::endl;
            }
            return result;
            
        } catch (const std::exception& e) {
            std::cout << "[警告] LLM応答のパースに失敗（試行 " << retry + 1 << "/" 
                      << LLM_PARSE_RETRY_COUNT << "）: " << e.what() << std::endl;
            
            // 最後のリトライでも失敗した場合、キーワードベースにフォールバック
            if (retry == LLM_PARSE_RETRY_COUNT - 1) {
                std::cout << "[情報] キーワードベース分析にフォールバックします" << std::endl;
                return analyze_with_keywords(user_input);
            }
            
            // リトライ前に短い待機時間を入れる（オプション）
            // std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // ここには到達しないはずだが、念のためフォールバック
    return analyze_with_keywords(user_input);
}

std::string InputAnalyzer::create_analysis_prompt(const std::string& user_input,
                                                  const std::deque<ConversationTurn>* recent_history) {
    std::string prompt =
        "あなたはユーザー発話を厳密JSONに変換する分析器です。\n"
        "出力はJSONオブジェクト1個のみ。説明文・コードブロックは禁止。\n\n";

    // 会話履歴がある場合は追加
    if (recent_history != nullptr && !recent_history->empty()) {
                prompt += "【最近の会話履歴】\n";
        int turns_to_show = std::min(INPUT_ANALYZER_HISTORY_TURNS_TO_SHOW,
                                     static_cast<int>(recent_history->size()));
        auto start_iter = recent_history->end() - turns_to_show;
        
        for (auto it = start_iter; it != recent_history->end(); ++it) {
                        prompt += it->role + ": " + normalize_analysis_text(it->content) + "\n";
        }
        prompt += "\n";
    }

        prompt += "【ユーザー発言】\n" + normalize_analysis_text(user_input) + "\n\n";
        prompt +=
                "【判定ルール】\n"
                "- intentは praise|criticism|question|greeting|casual のみ。\n"
                "- AIの回答品質への不満/改善要求は criticism。\n"
                "- AIへの明確な否定評価があれば evaluation_to_ai=negative。\n"
                "- 指示語（それ/これ）は会話履歴を参照して topic を具体化。\n"
                "- sentiment_score は -1.0〜1.0。\n\n"
                "【出力スキーマ】\n"
                "{\n"
                "  \"topic\": \"...\",\n"
                "  \"intent\": \"praise|criticism|question|greeting|casual\",\n"
                "  \"evaluation_to_ai\": \"positive|neutral|negative\",\n"
                "  \"keywords\": [\"...\"],\n"
                "  \"sentiment_score\": 0.0\n"
                "}\n\n"
                "【制約】\n"
                "- JSONオブジェクト1個のみを返す。\n"
                "- 前置き/解説/Markdown/コードブロックは禁止。\n"
                "- 必須キーが欠ける出力は禁止。\n";
        
    return prompt;
}

AnalyzedInput InputAnalyzer::parse_llm_response(
    const std::string& llm_output, 
    const std::string& raw_text) {
    
    AnalyzedInput result;
    result.raw_text = raw_text;
    
    const std::string json_str = extract_first_json_object(llm_output);
    if (json_str.empty()) {
        throw std::runtime_error("JSONオブジェクトを抽出できません");
    }

    std::smatch match;

    const std::regex topic_regex(R"("topic"\s*:\s*"((?:\\.|[^"\\])*)")");
    if (std::regex_search(json_str, match, topic_regex) && match.size() > 1) {
        result.topic = trim_copy(unescape_json_string(match[1].str()));
    }

    const std::regex intent_regex(R"("intent"\s*:\s*"((?:\\.|[^"\\])*)")");
    if (std::regex_search(json_str, match, intent_regex) && match.size() > 1) {
        result.intent = intent_from_string(trim_copy(unescape_json_string(match[1].str())));
    }

    const std::regex eval_regex(R"("evaluation_to_ai"\s*:\s*"((?:\\.|[^"\\])*)")");
    if (std::regex_search(json_str, match, eval_regex) && match.size() > 1) {
        result.evaluation_to_ai = evaluation_from_string(trim_copy(unescape_json_string(match[1].str())));
    }

    const std::regex sentiment_regex(R"("sentiment_score"\s*:\s*(-?\d+(?:\.\d+)?))");
    if (std::regex_search(json_str, match, sentiment_regex) && match.size() > 1) {
        result.sentiment_score = std::stod(match[1].str());
        result.sentiment_score = std::max(INPUT_ANALYZER_SENTIMENT_MIN,
                                          std::min(INPUT_ANALYZER_SENTIMENT_MAX, result.sentiment_score));
    }

    const std::regex keywords_regex(R"("keywords"\s*:\s*\[((?:.|\n)*?)\])");
    if (std::regex_search(json_str, match, keywords_regex) && match.size() > 1) {
        const std::string keywords_str = match[1].str();
        const std::regex keyword_regex(R"("((?:\\.|[^"\\])*)")");
        auto keywords_begin = std::sregex_iterator(keywords_str.begin(), keywords_str.end(), keyword_regex);
        auto keywords_end = std::sregex_iterator();

        for (std::sregex_iterator i = keywords_begin; i != keywords_end; ++i) {
            std::smatch keyword_match = *i;
            if (keyword_match.size() > 1) {
                const std::string kw = trim_copy(unescape_json_string(keyword_match[1].str()));
                if (!kw.empty()) {
                    result.keywords.push_back(kw);
                }
            }
        }
    }

    const std::string missing_message = find_missing_field_message(result);
    if (!missing_message.empty()) {
        throw std::runtime_error(missing_message);
    }
    
    return result;
}

double InputAnalyzer::calculate_sentiment(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    int positive_count = 0;
    int negative_count = 0;

    // ポジティブキーワードのカウント
    for (const auto& keyword : positive_keywords_) {
        if (lower_text.find(keyword) != std::string::npos) {
            positive_count++;
        }
    }

    // ネガティブキーワードのカウント
    for (const auto& keyword : negative_keywords_) {
        if (lower_text.find(keyword) != std::string::npos) {
            negative_count++;
        }
    }

    // スコア計算（-1.0 ~ 1.0）
    int total = positive_count + negative_count;
    if (total == 0) {
        return 0.0; // ニュートラル
    }

    double score = static_cast<double>(positive_count - negative_count) / total;
    return std::max(INPUT_ANALYZER_SENTIMENT_MIN,
                    std::min(INPUT_ANALYZER_SENTIMENT_MAX, score));
}

Intent InputAnalyzer::classify_intent(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // 賞賛の検出
    for (const auto& keyword : praise_keywords_) {
        if (lower_text.find(keyword) != std::string::npos) {
            return Intent::PRAISE;
        }
    }

    // 批判の検出
    for (const auto& keyword : criticism_keywords_) {
        if (lower_text.find(keyword) != std::string::npos) {
            return Intent::CRITICISM;
        }
    }

    // 質問の検出（疑問符や疑問詞）
    const bool has_question_marker =
        text.find("?") != std::string::npos ||
        text.find("？") != std::string::npos;
    const std::vector<std::string> question_markers = { INPUT_ANALYZER_QUESTION_MARKER_KEYWORDS };
    const bool has_question_keyword = std::any_of(
        question_markers.begin(), question_markers.end(),
        [&](const std::string& marker) {
            return lower_text.find(marker) != std::string::npos;
        });

    if (has_question_marker || has_question_keyword) {
        return Intent::QUESTION;
    }

    // 挨拶の検出
    const std::vector<std::string> greeting_markers = { INPUT_ANALYZER_GREETING_KEYWORDS };
    const bool has_greeting = std::any_of(
        greeting_markers.begin(), greeting_markers.end(),
        [&](const std::string& marker) {
            return lower_text.find(marker) != std::string::npos;
        });

    if (has_greeting) {
        return Intent::GREETING;
    }

    return Intent::CASUAL; // その他の雑談
}

EvaluationToAI InputAnalyzer::evaluate_ai_attitude(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    int positive_count = 0;
    int negative_count = 0;

    // 賞賛の検出
    for (const auto& keyword : praise_keywords_) {
        if (lower_text.find(keyword) != std::string::npos) {
            positive_count++;
        }
    }

    // 批判の検出
    for (const auto& keyword : criticism_keywords_) {
        if (lower_text.find(keyword) != std::string::npos) {
            negative_count++;
        }
    }

    if (positive_count > negative_count) {
        return EvaluationToAI::POSITIVE;
    } else if (negative_count > positive_count) {
        return EvaluationToAI::NEGATIVE;
    }

    return EvaluationToAI::NEUTRAL;
}

std::vector<std::string> InputAnalyzer::extract_keywords(const std::string& text) {
    std::vector<std::string> keywords;
    std::istringstream iss(text);
    std::string word;

    // 簡易的な単語分割（空白区切り）
    while (iss >> word) {
        // 記号を除去
        word.erase(std::remove_if(word.begin(), word.end(), 
            [](unsigned char c) { return std::ispunct(c) != 0; }), word.end());
        
        // 長さが3文字以上の単語のみを抽出
        if (word.length() >= INPUT_ANALYZER_MIN_KEYWORD_LENGTH) {
            keywords.push_back(word);
        }
    }

    return keywords;
}

std::string InputAnalyzer::extract_topic(const std::string& text) {
    // 簡易実装：最初の名詞句や重要そうな単語を抽出
    // 実際にはより高度な自然言語処理が必要
    auto keywords = extract_keywords(text);
    
    if (!keywords.empty()) {
        return keywords[0]; // 最初のキーワードをトピックとする
    }
    
    return INPUT_ANALYZER_DEFAULT_TOPIC; // デフォルト
}
