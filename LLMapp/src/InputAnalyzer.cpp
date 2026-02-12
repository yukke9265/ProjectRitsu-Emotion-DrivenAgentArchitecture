#include "InputAnalyzer.h"
#include "LLMInference.h"
#include "Config.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <regex>
#include <iostream>

InputAnalyzer::InputAnalyzer() 
    : llm_inference_(nullptr)
    , use_llm_(false) {
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

AnalyzedInput InputAnalyzer::analyze(const std::string& user_input) {
    // ハイブリッドモード：LLMとキーワードベースの両方を試行
    if (use_llm_ && llm_inference_ != nullptr) {
        try {
            // LLMベースの分析を試行
            AnalyzedInput llm_result = analyze_with_llm(user_input);
            
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

AnalyzedInput InputAnalyzer::analyze_with_llm(const std::string& user_input) {
    // プロンプトを生成
    std::string prompt = create_analysis_prompt(user_input);
    
    // パース成功まで最大LLM_PARSE_RETRY_COUNT回リトライ
    for (int retry = 0; retry < LLM_PARSE_RETRY_COUNT; retry++) {
        try {
            // LLMで単発推論（KVキャッシュは自動クリア）
            std::string llm_output = llm_inference_->infer_stateless(prompt);
            
            // LLM応答をパースして構造化データに変換
            AnalyzedInput result = parse_llm_response(llm_output, user_input);
            
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

std::string InputAnalyzer::create_analysis_prompt(const std::string& user_input) {
    return R"(あなたはユーザー入力を構造化分析する専門家です。
以下のユーザー発言を分析し、JSON形式で出力してください。

【ユーザー発言】
)" + user_input + R"(

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

【分析例】
入力: "助かります。ただ、もう少し詳細が知りたいです"
→ {"topic":"情報の詳細度", "intent":"question", "evaluation_to_ai":"neutral", "keywords":["助かる","詳細","知りたい"], "sentiment_score":0.3}

入力: "ふーん。なるほどね"
→ {"topic":"相槌", "intent":"casual", "evaluation_to_ai":"neutral", "keywords":["相槌","なるほど"], "sentiment_score":0.1}

入力: "最近暑くなってきましたね"
→ {"topic":"気候", "intent":"casual", "evaluation_to_ai":"neutral", "keywords":["最近","暑い","気候"], "sentiment_score":0.2}

入力: "まあまあだけど、もっと具体例があるといいな"
→ {"topic":"具体例の不足", "intent":"criticism", "evaluation_to_ai":"neutral", "keywords":["まあまあ","具体例","欲しい"], "sentiment_score":-0.3}

【出力】
JSON形式のみを出力してください。説明や追加の文章は不要です。
)";
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
        
        // 簡易的なJSONパース（正規表現ベース）
        // topic の抽出
        std::regex topic_regex(R"xxx("topic"\s*:\s*"([^"]*)")xxx");
        if (std::regex_search(json_str, match, topic_regex) && match.size() > 1) {
            result.topic = match[1].str();
        } else {
            result.topic = "general";
        }
        
        // intent の抽出
        std::regex intent_regex(R"xxx("intent"\s*:\s*"([^"]*)")xxx");
        if (std::regex_search(json_str, match, intent_regex) && match.size() > 1) {
            result.intent = match[1].str();
        } else {
            result.intent = "casual";
        }
        
        // evaluation_to_ai の抽出
        std::regex eval_regex(R"xxx("evaluation_to_ai"\s*:\s*"([^"]*)")xxx");
        if (std::regex_search(json_str, match, eval_regex) && match.size() > 1) {
            result.evaluation_to_ai = match[1].str();
        } else {
            result.evaluation_to_ai = "neutral";
        }
        
        // sentiment_score の抽出
        std::regex sentiment_regex(R"("sentiment_score"\s*:\s*(-?\d+\.?\d*))");
        if (std::regex_search(json_str, match, sentiment_regex) && match.size() > 1) {
            result.sentiment_score = std::stod(match[1].str());
            // 範囲チェック
            result.sentiment_score = std::max(-1.0, std::min(1.0, result.sentiment_score));
        } else {
            result.sentiment_score = 0.0;
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
        }
        
        // キーワードが抽出できなかった場合のフォールバック
        if (result.keywords.empty()) {
            result.keywords = extract_keywords(raw_text);
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
