#include "InputAnalyzer.h"
#include "LLMInference.h"
#include "MemoryController.h"
#include "Config.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <regex>
#include <iostream>

namespace {
bool is_valid_intent(const std::string& intent) {
    return intent == "praise" || intent == "criticism" || intent == "question" ||
           intent == "greeting" || intent == "casual";
}

bool is_valid_evaluation(const std::string& evaluation) {
    return evaluation == "positive" || evaluation == "neutral" || evaluation == "negative";
}

bool is_semantically_valid_llm_result(const AnalyzedInput& result) {
    if (result.topic.empty()) return false;
    if (!is_valid_intent(result.intent)) return false;
    if (!is_valid_evaluation(result.evaluation_to_ai)) return false;
    if (result.sentiment_score < -1.0 || result.sentiment_score > 1.0) return false;
    return true;
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
    positive_keywords_ = {
        "good", "great", "excellent", "amazing", "wonderful",
        "すごい", "良い", "素晴らしい", "最高", "嬉しい", "楽しい",
        "ありがとう", "感謝", "助かる"
    };

    // ネガティブキーワード
    negative_keywords_ = {
        "bad", "terrible", "awful", "horrible", "wrong",
        "悪い", "ひどい", "最悪", "嫌", "つまらない", "不快",
        "違う", "間違い", "ダメ"
    };

    // 賞賛キーワード
    praise_keywords_ = {
        "すごい", "素晴らしい", "賢い", "天才", "優秀",
        "ありがとう", "感謝", "役立つ", "助かる",
        "smart", "brilliant", "genius", "helpful", "thanks"
    };

    // 批判キーワード
    criticism_keywords_ = {
        "ダメ", "使えない", "バカ", "無能", "役立たず",
        "最悪", "ひどい", "間違い", "違う",
        "stupid", "useless", "terrible", "wrong", "bad"
    };
}

AnalyzedInput InputAnalyzer::analyze(const std::string& user_input,
                                     const std::deque<ConversationTurn>* recent_history) {
    // ハイブリッドモード：LLMとキーワードベースの両方を試行
    if (use_llm_ && llm_inference_ != nullptr) {
        try {
            // LLMベースの分析を試行
            AnalyzedInput llm_result = analyze_with_llm(user_input, recent_history);
            
            // LLM結果の妥当性チェック（基本的な検証）
            if (!llm_result.intent.empty() && 
                llm_result.sentiment_score >= -1.0 && 
                llm_result.sentiment_score <= 1.0) {
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
    return analyze_with_keywords(user_input);
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
        if (prompt.length() > 500) {
            std::cout << "  " << prompt.substr(0, 250) << "\n";
            std::cout << "  [...省略 " << (prompt.length() - 500) << " 文字...]\n";
            std::cout << "  " << prompt.substr(prompt.length() - 250) << "\n";
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
    std::string prompt = R"(あなたはユーザー入力を構造化分析する専門家です。
以下のユーザー発言を分析し、JSON形式で出力してください。

)";

    // 会話履歴がある場合は追加
    if (recent_history != nullptr && !recent_history->empty()) {
        prompt += "【最近の会話履歴】\n";
        // 最新5ターンまでを取得
        int turns_to_show = std::min(5, static_cast<int>(recent_history->size()));
        auto start_iter = recent_history->end() - turns_to_show;
        
        for (auto it = start_iter; it != recent_history->end(); ++it) {
            prompt += it->role + ": " + it->content + "\n";
        }
        prompt += "\n";
    }

    prompt += "【ユーザー発言】\n" + user_input + R"(

【会話履歴を考慮した分析の重要ルール】
1. **指示語の解釈**: 「それ」「これ」「その話」などは会話履歴から参照先を特定し、topicに反映
2. **前の回答への反応**: 
   - 「もっと〜して」「〜してください」「〜がない」「〜が足りない」などの改善要求は criticism
   - 前の回答への不満表現（「わからない」「抽象的」「不十分」）は evaluation=negative
3. **話題転換の検出**: 
   - 「ところで」「さて」「それはそうと」「話は変わるけど」で始まる発言は casual（新しい話題）
   - 前の会話と無関係な内容の場合も casual
4. **継続的な話題**: 会話履歴の内容を引き継ぐ場合、topicは履歴の文脈を含める

【出力形式】
{
  "topic": "発言のトピック（何について話しているか）",
  "intent": "意図（praise/criticism/question/greeting/casual のいずれか）",
  "evaluation_to_ai": "AIへの評価（positive/neutral/negative のいずれか）",
  "keywords": ["キーワード1", "キーワード2", "キーワード3"],
  "sentiment_score": 0.0
}

【各フィールドの判定基準】

◆ intent（発言の主な目的）:
  - praise: AIを褒める、感謝する、評価する（「すごい」「ありがとう」「助かった」）
  - criticism: AIを批判する、不満を述べる、改善要求（「ダメ」「違う」「もっとちゃんと」）
  - question: 情報を求める、疑問を投げかける（「〜は何ですか？」「どうやって？」「本当に？」）
  - greeting: 挨拶、呼びかけ（「こんにちは」「よろしく」「おはよう」）- AIとの関係構築が目的
  - casual: 雑談、日常会話（「良い天気」「そうなんだ」）- 情報交換や感想の共有

◆ evaluation_to_ai（AI自身に対する評価・態度）:
  - positive: AIの能力や応答を明示的に評価・称賛している
  - negative: AIの能力や応答を明示的に批判・否定している
  - neutral: AIへの評価が含まれない、または中立的
  ※注意: 発言全体の雰囲気ではなく「AI自身への評価」に焦点を当てる

◆ sentiment_score（発言全体の感情の強さ）:
  - 1.0: 非常に強い喜び・興奮・感謝
  - 0.7～0.9: 明確なポジティブ感情
  - 0.3～0.6: 穏やかなポジティブ
  - 0.0～0.2: ほぼ中立、淡々とした表現
  - -0.3～-0.6: やや不満やネガティブ
  - -0.7～-0.9: 明確な不満・批判
  - -1.0: 強い怒り・拒絶
  ※混合感情（「悪くないけど」「ありがとう。でも」）は中間値（-0.3～0.3）

【重要な注意事項】
1. 皮肉や社交辞令に注意: 短く淡々とした称賛（「へぇ」「すごいね」のみ）は0.0～0.3程度に抑える
2. 挨拶の感情値: 「こんにちは」「よろしく」だけなら0.0～0.3（明るい修飾語があれば0.5～0.8）
3. 混合感情: 肯定と否定が混在する場合、より強い方に寄せるが極端にしない（-0.6～0.6）
4. 質問のトーン: 疑念を含む質問（「本当に？」）は evaluation=neutral, sentiment=0.0～0.3
5. 改善要求は criticism: 「もっと〜して」「〜が足りない」「〜がない」などは批判として扱う

【分析例】

■ 単独の発言（会話履歴なし）
入力: "助かります。ただ、もう少し詳細が知りたいです"
→ {"topic":"情報の詳細度", "intent":"question", "evaluation_to_ai":"neutral", "keywords":["助かる","詳細","知りたい"], "sentiment_score":0.3}

入力: "ふーん。なるほどね"
→ {"topic":"相槌", "intent":"casual", "evaluation_to_ai":"neutral", "keywords":["相槌","なるほど"], "sentiment_score":0.1}

入力: "最近暑くなってきましたね"
→ {"topic":"気候", "intent":"casual", "evaluation_to_ai":"neutral", "keywords":["最近","暑い","気候"], "sentiment_score":0.2}

入力: "まあまあだけど、もっと具体例があるといいな"
→ {"topic":"具体例の不足", "intent":"criticism", "evaluation_to_ai":"neutral", "keywords":["まあまあ","具体例","欲しい"], "sentiment_score":-0.3}

■ 会話履歴を考慮した分析（重要）
会話履歴: "user: Pythonの辞書について教えて" → "assistant: 辞書はキーと値のペアを格納します"
入力: "それはわかった。具体例を教えて"
→ {"topic":"Pythonの辞書の具体例", "intent":"question", "evaluation_to_ai":"neutral", "keywords":["わかった","具体例","辞書"], "sentiment_score":0.1}
※「それ」=Pythonの辞書を指す

会話履歴: "user: 機械学習について" → "assistant: データからパターンを学習する技術です"
入力: "もっと具体的に説明してよ。そんな抽象的な説明じゃわからない"
→ {"topic":"機械学習の説明の不満", "intent":"criticism", "evaluation_to_ai":"negative", "keywords":["具体的","抽象的","わからない"], "sentiment_score":-0.5}
※前の回答への不満 → criticism + negative

会話履歴: "user: C++のポインタについて" → "assistant: メモリアドレスを格納する変数です"
入力: "ところで、今日は良い天気だね"
→ {"topic":"天気", "intent":"casual", "evaluation_to_ai":"neutral", "keywords":["ところで","天気","良い"], "sentiment_score":0.2}
※「ところで」は話題転換のマーカー → casual

会話履歴: "user: メモリリークって何？" → "assistant: メモリを解放し忘れることです" → "user: 防ぐ方法は？" → "assistant: スマートポインタを使います"
入力: "なるほど！それすごく便利そう"
→ {"topic":"スマートポインタの有用性", "intent":"praise", "evaluation_to_ai":"neutral", "keywords":["なるほど","便利","スマートポインタ"], "sentiment_score":0.6}
※「それ」=スマートポインタを指す、ポジティブな反応 → praise

【出力の絶対ルール（厳守）】
1. 出力はJSONオブジェクト1個のみ（先頭は"{"、末尾は"}"）
2. JSONの前後に文字を一切付けない（前置き・注釈・説明・謝罪・提案を禁止）
3. Markdown記法を禁止（```json, ```, 箇条書き, 矢印, 絵文字を禁止）
4. 必須フィールド topic / intent / evaluation_to_ai / keywords / sentiment_score を必ず埋める
5. intentは praise|criticism|question|greeting|casual のいずれか
6. evaluation_to_aiは positive|neutral|negative のいずれか
7. keywords は1件以上の文字列配列
8. sentiment_score は -1.0 から 1.0 の数値

【出力】
JSONオブジェクトのみを出力してください。
)";
        
    return prompt;
}

AnalyzedInput InputAnalyzer::parse_llm_response(
    const std::string& llm_output, 
    const std::string& raw_text) {
    
    AnalyzedInput result;
    result.raw_text = raw_text;
    
    try {
        // JSONブロックを抽出（{...}の部分）
        std::regex json_regex(R"(\{[^{}]*(?:\{[^{}]*\}[^{}]*)*\})");
        std::smatch match;
        std::string json_str;
        
        if (std::regex_search(llm_output, match, json_regex)) {
            json_str = match.str();
        } else {
            throw std::runtime_error("JSON形式が見つかりません");
        }
        
        // 厳格JSONパース（必須項目が欠けたら失敗）
        bool has_topic = false;
        bool has_intent = false;
        bool has_evaluation = false;
        bool has_sentiment = false;
        bool has_keywords = false;

        // topic の抽出
        std::regex topic_regex(R"xxx("topic"\s*:\s*"([^"]*)")xxx");
        if (std::regex_search(json_str, match, topic_regex) && match.size() > 1) {
            result.topic = match[1].str();
            has_topic = !result.topic.empty();
        }
        
        // intent の抽出
        std::regex intent_regex(R"xxx("intent"\s*:\s*"([^"]*)")xxx");
        if (std::regex_search(json_str, match, intent_regex) && match.size() > 1) {
            result.intent = match[1].str();
            has_intent = !result.intent.empty();
        }
        
        // evaluation_to_ai の抽出
        std::regex eval_regex(R"xxx("evaluation_to_ai"\s*:\s*"([^"]*)")xxx");
        if (std::regex_search(json_str, match, eval_regex) && match.size() > 1) {
            result.evaluation_to_ai = match[1].str();
            has_evaluation = !result.evaluation_to_ai.empty();
        }
        
        // sentiment_score の抽出
        std::regex sentiment_regex(R"("sentiment_score"\s*:\s*(-?\d+\.?\d*))");
        if (std::regex_search(json_str, match, sentiment_regex) && match.size() > 1) {
            result.sentiment_score = std::stod(match[1].str());
            // 範囲チェック
            result.sentiment_score = std::max(-1.0, std::min(1.0, result.sentiment_score));
            has_sentiment = true;
        }
        
        // keywords の抽出
        std::regex keywords_regex(R"("keywords"\s*:\s*\[([^\]]*)\])");
        if (std::regex_search(json_str, match, keywords_regex) && match.size() > 1) {
            std::string keywords_str = match[1].str();
            std::regex keyword_regex(R"xxx("([^"]*)")xxx");
            auto keywords_begin = std::sregex_iterator(keywords_str.begin(), keywords_str.end(), keyword_regex);
            auto keywords_end = std::sregex_iterator();
            
            for (std::sregex_iterator i = keywords_begin; i != keywords_end; ++i) {
                std::smatch keyword_match = *i;
                if (keyword_match.size() > 1) {
                    result.keywords.push_back(keyword_match[1].str());
                }
            }
            has_keywords = !result.keywords.empty();
        }

        // 必須項目の検証（不足時は再生成リトライへ）
        if (!has_topic || !has_intent || !has_evaluation || !has_sentiment || !has_keywords) {
            throw std::runtime_error("必須JSONフィールドが不足または空です");
        }
        
    } catch (const std::exception& e) {
        // パース失敗 - 例外を再スローして上位でリトライ処理させる
        throw;
    }
    
    return result;
}

double InputAnalyzer::calculate_sentiment(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);

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
    return std::max(-1.0, std::min(1.0, score));
}

std::string InputAnalyzer::classify_intent(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);

    // 賞賛の検出
    for (const auto& keyword : praise_keywords_) {
        if (lower_text.find(keyword) != std::string::npos) {
            return "praise";
        }
    }

    // 批判の検出
    for (const auto& keyword : criticism_keywords_) {
        if (lower_text.find(keyword) != std::string::npos) {
            return "criticism";
        }
    }

    // 質問の検出（疑問符や疑問詞）
    if (text.find("?") != std::string::npos ||
        text.find("？") != std::string::npos ||
        lower_text.find("what") != std::string::npos ||
        lower_text.find("how") != std::string::npos ||
        lower_text.find("why") != std::string::npos ||
        lower_text.find("なぜ") != std::string::npos ||
        lower_text.find("どう") != std::string::npos ||
        lower_text.find("何") != std::string::npos) {
        return "question";
    }

    // 挨拶の検出
    if (lower_text.find("hello") != std::string::npos ||
        lower_text.find("hi") != std::string::npos ||
        lower_text.find("こんにちは") != std::string::npos ||
        lower_text.find("おはよう") != std::string::npos ||
        lower_text.find("こんばんは") != std::string::npos) {
        return "greeting";
    }

    return "casual"; // その他の雑談
}

std::string InputAnalyzer::evaluate_ai_attitude(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);

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
        return "positive";
    } else if (negative_count > positive_count) {
        return "negative";
    }

    return "neutral";
}

std::vector<std::string> InputAnalyzer::extract_keywords(const std::string& text) {
    std::vector<std::string> keywords;
    std::istringstream iss(text);
    std::string word;

    // 簡易的な単語分割（空白区切り）
    while (iss >> word) {
        // 記号を除去
        word.erase(std::remove_if(word.begin(), word.end(), 
            [](char c) { return std::ispunct(c); }), word.end());
        
        // 長さが3文字以上の単語のみを抽出
        if (word.length() >= 3) {
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
    
    return "general"; // デフォルト
}
