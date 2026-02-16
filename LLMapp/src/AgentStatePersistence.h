#pragma once

#include "EmotionEngine.h"
#include "MemoryController.h"
#include "PromptOrchestrator.h"
#include <string>
#include <vector>

/**
 * @brief 永続化対象となるエージェント状態
 */
struct AgentPersistentState {
    PersonalityConstitution constitution;
    EmotionState emotion_state;

    int short_term_limit = 10;
    std::deque<ConversationTurn> short_term_memory;
    std::vector<Episode> long_term_memory;

    std::string system_prompt;
    std::string tone_instruction;
    int max_episodes = 3;
    int short_term_turns = 5;
    std::vector<PromptOrchestrator::StructuredLogSection> system_log_sections;

    bool debug_mode = false;
};

/**
 * @brief エージェント状態のファイル保存/読込
 *
 * フォーマット運用ポリシー:
 * - 保存は常に最新版フォーマットで書き込む
 * - 読み込みは旧版との後方互換を維持する
 * - 新版追加時は既存読み込み経路を削除せず拡張する
 */
class AgentStatePersistence {
public:
    /**
     * @brief エージェント状態を保存（最新版フォーマットで保存）
     */
    static bool save_to_file(const std::string& file_path, const AgentPersistentState& state);

    /**
     * @brief エージェント状態を読込（旧版フォーマットも可能な範囲で復元）
     */
    static bool load_from_file(const std::string& file_path, AgentPersistentState& out_state);
};
