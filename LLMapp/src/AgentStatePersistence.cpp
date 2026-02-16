#include "AgentStatePersistence.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <type_traits>

namespace {

constexpr char kStateMagic[] = "EMOTIONAL_AGENT_STATE_BIN";
// フォーマット版。新規保存は常にこの版で書き込む。
// 読み込み側は switch(version) で旧版を維持しつつ拡張する。
constexpr uint32_t kStateVersion = 2;

std::array<BasicEmotion, 8> all_emotions() {
    return {
        BasicEmotion::JOY,
        BasicEmotion::TRUST,
        BasicEmotion::FEAR,
        BasicEmotion::SURPRISE,
        BasicEmotion::SADNESS,
        BasicEmotion::DISGUST,
        BasicEmotion::ANGER,
        BasicEmotion::ANTICIPATION
    };
}

int64_t to_unix_seconds(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point from_unix_seconds(int64_t unix_seconds) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(unix_seconds));
}

template <typename T>
bool write_pod(std::ofstream& ofs, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "POD only");
    ofs.write(reinterpret_cast<const char*>(&value), static_cast<std::streamsize>(sizeof(T)));
    return ofs.good();
}

template <typename T>
bool read_pod(std::ifstream& ifs, T& value) {
    static_assert(std::is_trivially_copyable_v<T>, "POD only");
    ifs.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(T)));
    return ifs.good();
}

bool write_string(std::ofstream& ofs, const std::string& value) {
    uint64_t size = static_cast<uint64_t>(value.size());
    if (!write_pod(ofs, size)) {
        return false;
    }
    if (size > 0) {
        ofs.write(value.data(), static_cast<std::streamsize>(size));
    }
    return ofs.good();
}

bool read_string(std::ifstream& ifs, std::string& out_value) {
    uint64_t size = 0;
    if (!read_pod(ifs, size)) {
        return false;
    }

    out_value.clear();
    if (size == 0) {
        return true;
    }

    out_value.resize(static_cast<size_t>(size));
    ifs.read(out_value.data(), static_cast<std::streamsize>(size));
    if (!ifs.good()) {
        return false;
    }

    return true;
}

bool write_state_payload_v1(std::ofstream& ofs, const AgentPersistentState& state) {
    // v1: 初期導入時の基本ペイロード
    // 構成: constitution -> emotion -> memories -> prompt settings -> debug_mode
    if (!write_string(ofs, state.constitution.core_values)) return false;
    if (!write_string(ofs, state.constitution.communication_style)) return false;
    if (!write_pod(ofs, state.constitution.sensitivity_to_praise)) return false;
    if (!write_pod(ofs, state.constitution.sensitivity_to_criticism)) return false;
    if (!write_pod(ofs, state.constitution.decay_rate)) return false;
    if (!write_pod(ofs, state.constitution.baseline_valence)) return false;

    for (const auto emotion : all_emotions()) {
        const auto it = state.emotion_state.values.find(emotion);
        const double value = (it != state.emotion_state.values.end()) ? it->second : 0.0;
        if (!write_pod(ofs, value)) return false;
    }
    if (!write_pod(ofs, state.emotion_state.overall_valence)) return false;
    if (!write_pod(ofs, state.emotion_state.arousal)) return false;
    const int64_t emotion_ts = to_unix_seconds(state.emotion_state.last_update);
    if (!write_pod(ofs, emotion_ts)) return false;

    if (!write_pod(ofs, state.short_term_limit)) return false;

    const uint64_t short_term_count = static_cast<uint64_t>(state.short_term_memory.size());
    if (!write_pod(ofs, short_term_count)) return false;
    for (const auto& turn : state.short_term_memory) {
        if (!write_string(ofs, turn.role)) return false;
        if (!write_string(ofs, turn.content)) return false;
        const int64_t turn_ts = to_unix_seconds(turn.timestamp);
        if (!write_pod(ofs, turn_ts)) return false;
    }

    const uint64_t long_term_count = static_cast<uint64_t>(state.long_term_memory.size());
    if (!write_pod(ofs, long_term_count)) return false;
    for (const auto& episode : state.long_term_memory) {
        if (!write_string(ofs, episode.summary)) return false;
        if (!write_string(ofs, episode.emotional_tag)) return false;
        if (!write_pod(ofs, episode.importance)) return false;
        const int64_t episode_ts = to_unix_seconds(episode.timestamp);
        if (!write_pod(ofs, episode_ts)) return false;

        const uint64_t keyword_count = static_cast<uint64_t>(episode.keywords.size());
        if (!write_pod(ofs, keyword_count)) return false;
        for (const auto& keyword : episode.keywords) {
            if (!write_string(ofs, keyword)) return false;
        }
    }

    if (!write_string(ofs, state.system_prompt)) return false;
    if (!write_string(ofs, state.tone_instruction)) return false;
    if (!write_pod(ofs, state.max_episodes)) return false;
    if (!write_pod(ofs, state.short_term_turns)) return false;

    const uint64_t section_count = static_cast<uint64_t>(state.system_log_sections.size());
    if (!write_pod(ofs, section_count)) return false;
    for (const auto& section : state.system_log_sections) {
        if (!write_string(ofs, section.name)) return false;
        if (!write_string(ofs, section.content)) return false;
    }

    const uint8_t debug_mode = state.debug_mode ? 1 : 0;
    if (!write_pod(ofs, debug_mode)) return false;

    return ofs.good();
}

bool read_state_payload_v1(std::ifstream& ifs, AgentPersistentState& loaded) {
    // v1 読み込みは後方互換のため維持する
    if (!read_string(ifs, loaded.constitution.core_values)) return false;
    if (!read_string(ifs, loaded.constitution.communication_style)) return false;
    if (!read_pod(ifs, loaded.constitution.sensitivity_to_praise)) return false;
    if (!read_pod(ifs, loaded.constitution.sensitivity_to_criticism)) return false;
    if (!read_pod(ifs, loaded.constitution.decay_rate)) return false;
    if (!read_pod(ifs, loaded.constitution.baseline_valence)) return false;

    loaded.emotion_state = EmotionState();
    for (const auto emotion : all_emotions()) {
        double value = 0.0;
        if (!read_pod(ifs, value)) return false;
        loaded.emotion_state.values[emotion] = value;
    }
    if (!read_pod(ifs, loaded.emotion_state.overall_valence)) return false;
    if (!read_pod(ifs, loaded.emotion_state.arousal)) return false;
    int64_t emotion_ts = 0;
    if (!read_pod(ifs, emotion_ts)) return false;
    loaded.emotion_state.last_update = from_unix_seconds(emotion_ts);

    if (!read_pod(ifs, loaded.short_term_limit)) return false;

    uint64_t short_term_count = 0;
    if (!read_pod(ifs, short_term_count)) return false;
    loaded.short_term_memory.clear();
    loaded.short_term_memory.resize(static_cast<size_t>(short_term_count), ConversationTurn("", ""));
    for (uint64_t i = 0; i < short_term_count; ++i) {
        if (!read_string(ifs, loaded.short_term_memory[static_cast<size_t>(i)].role)) return false;
        if (!read_string(ifs, loaded.short_term_memory[static_cast<size_t>(i)].content)) return false;
        int64_t turn_ts = 0;
        if (!read_pod(ifs, turn_ts)) return false;
        loaded.short_term_memory[static_cast<size_t>(i)].timestamp = from_unix_seconds(turn_ts);
    }

    uint64_t long_term_count = 0;
    if (!read_pod(ifs, long_term_count)) return false;
    loaded.long_term_memory.clear();
    loaded.long_term_memory.resize(static_cast<size_t>(long_term_count));

    for (uint64_t i = 0; i < long_term_count; ++i) {
        auto& episode = loaded.long_term_memory[static_cast<size_t>(i)];
        if (!read_string(ifs, episode.summary)) return false;
        if (!read_string(ifs, episode.emotional_tag)) return false;
        if (!read_pod(ifs, episode.importance)) return false;
        int64_t episode_ts = 0;
        if (!read_pod(ifs, episode_ts)) return false;
        episode.timestamp = from_unix_seconds(episode_ts);

        uint64_t keyword_count = 0;
        if (!read_pod(ifs, keyword_count)) return false;
        episode.keywords.clear();
        episode.keywords.resize(static_cast<size_t>(keyword_count));
        for (uint64_t j = 0; j < keyword_count; ++j) {
            if (!read_string(ifs, episode.keywords[static_cast<size_t>(j)])) return false;
        }
    }

    if (!read_string(ifs, loaded.system_prompt)) return false;
    if (!read_string(ifs, loaded.tone_instruction)) return false;
    if (!read_pod(ifs, loaded.max_episodes)) return false;
    if (!read_pod(ifs, loaded.short_term_turns)) return false;

    uint64_t section_count = 0;
    if (!read_pod(ifs, section_count)) return false;
    loaded.system_log_sections.clear();
    loaded.system_log_sections.resize(static_cast<size_t>(section_count));
    for (uint64_t i = 0; i < section_count; ++i) {
        if (!read_string(ifs, loaded.system_log_sections[static_cast<size_t>(i)].name)) return false;
        if (!read_string(ifs, loaded.system_log_sections[static_cast<size_t>(i)].content)) return false;
    }

    uint8_t debug_mode = 0;
    if (!read_pod(ifs, debug_mode)) return false;
    loaded.debug_mode = (debug_mode != 0);

    return true;
}

bool write_state_payload_v2(std::ofstream& ofs, const AgentPersistentState& state) {
    // v2: v1の末尾に拡張フィールドを追加
    // 既存フィールド順は変えない（互換性維持）
    if (!write_state_payload_v1(ofs, state)) {
        return false;
    }

    // 将来の機能拡張ビット（現状は未使用）
    const uint32_t extension_flags = 0;
    if (!write_pod(ofs, extension_flags)) {
        return false;
    }

    // 保存時刻（将来のデバッグ/診断用途）
    const int64_t saved_at = to_unix_seconds(std::chrono::system_clock::now());
    if (!write_pod(ofs, saved_at)) {
        return false;
    }

    return ofs.good();
}

bool read_state_payload_v2(std::ifstream& ifs, AgentPersistentState& loaded) {
    // v2はv1を先に読み、末尾拡張を読み進める
    if (!read_state_payload_v1(ifs, loaded)) {
        return false;
    }

    uint32_t extension_flags = 0;
    if (!read_pod(ifs, extension_flags)) {
        return false;
    }

    int64_t saved_at = 0;
    if (!read_pod(ifs, saved_at)) {
        return false;
    }

    (void)extension_flags;
    (void)saved_at;

    return true;
}

} // namespace

bool AgentStatePersistence::save_to_file(const std::string& file_path, const AgentPersistentState& state) {
    std::ofstream ofs(file_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    const uint32_t magic_size = static_cast<uint32_t>(std::strlen(kStateMagic));
    if (!write_pod(ofs, magic_size)) return false;
    ofs.write(kStateMagic, static_cast<std::streamsize>(magic_size));
    if (!ofs.good()) return false;
    if (!write_pod(ofs, kStateVersion)) return false;

    return write_state_payload_v2(ofs, state);
}

bool AgentStatePersistence::load_from_file(const std::string& file_path, AgentPersistentState& out_state) {
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }

    uint32_t magic_size = 0;
    if (!read_pod(ifs, magic_size)) {
        return false;
    }

    std::string magic(static_cast<size_t>(magic_size), '\0');
    if (magic_size > 0) {
        ifs.read(magic.data(), static_cast<std::streamsize>(magic_size));
        if (!ifs.good()) {
            return false;
        }
    }
    if (magic != kStateMagic) {
        return false;
    }

    uint32_t version = 0;
    if (!read_pod(ifs, version)) {
        return false;
    }

    AgentPersistentState loaded;

    bool ok = false;
    // 新しい版を追加する際のルール:
    // 1) case 追加で旧caseは消さない
    // 2) write は最新版を使う
    // 3) 読み込み互換が必要なら旧版をこの switch で維持する
    switch (version) {
        case 1:
            ok = read_state_payload_v1(ifs, loaded);
            break;
        case 2:
            ok = read_state_payload_v2(ifs, loaded);
            break;
        default:
            return false;
    }

    if (!ok) {
        return false;
    }

    out_state = std::move(loaded);
    return true;
}
