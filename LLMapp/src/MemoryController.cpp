#include "MemoryController.h"
#include <algorithm>
#include <sstream>
#include <cmath>

MemoryController::MemoryController()
    : short_term_limit_(10) {
}

MemoryController::MemoryController(int short_term_limit)
    : short_term_limit_(short_term_limit) {
}

MemoryController::~MemoryController() {
}

// ===== 短期メモリ =====

void MemoryController::add_to_short_term(const std::string& role, const std::string& content) {
    short_term_memory_.emplace_back(role, content);

    // 制限を超えた場合、古いものから削除
    while (static_cast<int>(short_term_memory_.size()) > short_term_limit_) {
        short_term_memory_.pop_front();
    }
}

void MemoryController::clear_short_term() {
    short_term_memory_.clear();
}

std::string MemoryController::get_short_term_as_text(int max_turns) const {
    std::ostringstream oss;
    
    int start = 0;
    if (max_turns > 0 && static_cast<int>(short_term_memory_.size()) > max_turns) {
        start = static_cast<int>(short_term_memory_.size()) - max_turns;
    }

    for (int i = start; i < static_cast<int>(short_term_memory_.size()); ++i) {
        const auto& turn = short_term_memory_[i];
        oss << turn.role << ": " << turn.content << "\n";
    }

    return oss.str();
}

// ===== 長期メモリ =====

void MemoryController::add_to_long_term(const Episode& episode) {
    long_term_memory_.push_back(episode);

    // 重要度でソート（降順）
    std::sort(long_term_memory_.begin(), long_term_memory_.end(),
        [](const Episode& a, const Episode& b) {
            return a.importance > b.importance;
        });

    // 長期メモリのサイズ制限（例：最大100エピソード）
    const int max_episodes = 100;
    if (static_cast<int>(long_term_memory_.size()) > max_episodes) {
        long_term_memory_.resize(max_episodes);
    }
}

std::vector<Episode> MemoryController::search_episodes(
    const std::vector<std::string>& keywords,
    int max_results) const {
    
    // スコア付きエピソードのペア
    std::vector<std::pair<double, Episode>> scored_episodes;

    for (const auto& episode : long_term_memory_) {
        // キーワードの類似度を計算
        double similarity = calculate_similarity(keywords, episode.keywords);
        
        // 重要度も考慮したスコア
        double score = similarity * 0.7 + episode.importance * 0.3;
        
        if (score > 0.1) {  // 閾値
            scored_episodes.emplace_back(score, episode);
        }
    }

    // スコアでソート（降順）
    std::sort(scored_episodes.begin(), scored_episodes.end(),
        [](const auto& a, const auto& b) {
            return a.first > b.first;
        });

    // 上位max_results件を返す
    std::vector<Episode> results;
    int count = std::min(max_results, static_cast<int>(scored_episodes.size()));
    for (int i = 0; i < count; ++i) {
        results.push_back(scored_episodes[i].second);
    }

    return results;
}

std::vector<Episode> MemoryController::search_by_emotion(
    const std::string& emotional_tag,
    int max_results) const {
    
    std::vector<Episode> results;

    for (const auto& episode : long_term_memory_) {
        if (episode.emotional_tag == emotional_tag) {
            results.push_back(episode);
            if (static_cast<int>(results.size()) >= max_results) {
                break;
            }
        }
    }

    return results;
}

void MemoryController::clear_long_term() {
    long_term_memory_.clear();
}

void MemoryController::set_short_term_history(const std::deque<ConversationTurn>& history) {
    short_term_memory_ = history;

    while (static_cast<int>(short_term_memory_.size()) > short_term_limit_) {
        short_term_memory_.pop_front();
    }
}

void MemoryController::set_long_term_memory(const std::vector<Episode>& episodes) {
    long_term_memory_ = episodes;

    std::sort(long_term_memory_.begin(), long_term_memory_.end(),
        [](const Episode& a, const Episode& b) {
            return a.importance > b.importance;
        });

    const int max_episodes = 100;
    if (static_cast<int>(long_term_memory_.size()) > max_episodes) {
        long_term_memory_.resize(max_episodes);
    }
}

void MemoryController::consolidate_memory(
    const std::string& emotional_state,
    const std::vector<std::string>& keywords,
    const std::string& summary_override) {
    
    if (short_term_memory_.empty()) {
        return;
    }

    std::string summary_text = summary_override;

    if (summary_text.empty()) {
        // 短期メモリから会話を要約（フォールバック）
        std::ostringstream summary;
        summary << "会話の要約: ";
        
        for (const auto& turn : short_term_memory_) {
            summary << turn.role << "の発言 / ";
        }

        summary_text = summary.str();
    }

    // 感情タグを判定
    std::string emotional_tag = "neutral";
    if (emotional_state.find("ポジティブ") != std::string::npos ||
        emotional_state.find("喜び") != std::string::npos) {
        emotional_tag = "positive";
    } else if (emotional_state.find("ネガティブ") != std::string::npos ||
               emotional_state.find("悲しみ") != std::string::npos ||
               emotional_state.find("怒り") != std::string::npos) {
        emotional_tag = "negative";
    }

    // 重要度を計算
    bool has_strong_emotion = (emotional_tag != "neutral");
    double importance = calculate_importance(
        static_cast<int>(short_term_memory_.size()),
        has_strong_emotion);

    // エピソードを作成
    Episode episode(summary_text, emotional_tag, importance);
    episode.keywords = keywords;

    // 長期メモリに追加
    add_to_long_term(episode);
}

// ===== プライベートメソッド =====

double MemoryController::calculate_similarity(
    const std::vector<std::string>& keywords1,
    const std::vector<std::string>& keywords2) const {
    
    if (keywords1.empty() || keywords2.empty()) {
        return 0.0;
    }

    // 共通キーワードの数をカウント
    int common_count = 0;
    for (const auto& k1 : keywords1) {
        for (const auto& k2 : keywords2) {
            // 簡易的な部分一致判定
            if (k1.find(k2) != std::string::npos || k2.find(k1) != std::string::npos) {
                common_count++;
                break;
            }
        }
    }

    // Jaccard係数的な計算
    int total_unique = static_cast<int>(keywords1.size() + keywords2.size()) - common_count;
    if (total_unique == 0) {
        return 0.0;
    }

    return static_cast<double>(common_count) / total_unique;
}

double MemoryController::calculate_importance(
    int conversation_length,
    bool has_strong_emotion) const {
    
    // 会話の長さに基づくスコア（長いほど重要）
    double length_score = std::min(1.0, conversation_length / 10.0);

    // 強い感情があれば重要度を上げる
    double emotion_bonus = has_strong_emotion ? 0.3 : 0.0;

    // 合計スコア
    double importance = std::min(1.0, length_score * 0.7 + emotion_bonus);

    return importance;
}
