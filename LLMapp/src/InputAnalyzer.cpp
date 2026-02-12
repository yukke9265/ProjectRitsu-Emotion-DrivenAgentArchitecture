#include "InputAnalyzer.h"
#include <algorithm>
#include <sstream>
#include <cctype>

InputAnalyzer::InputAnalyzer() {
    initialize_dictionaries();
}

InputAnalyzer::~InputAnalyzer() {
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
    AnalyzedInput result;
    result.raw_text = user_input;

    // 各種分析を実行
    result.topic = extract_topic(user_input);
    result.intent = classify_intent(user_input);
    result.evaluation_to_ai = evaluate_ai_attitude(user_input);
    result.keywords = extract_keywords(user_input);
    result.sentiment_score = calculate_sentiment(user_input);

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
