#include "EmotionalAgent.h"
#include "DialogFunctions.h"
#include <iostream>
#include <string>

int main(int /*argc*/, char** /*argv*/) {
    // ===== 初期化 =====
    initialize_console();

    std::cout << "===== 感情駆動型AIエージェント =====\n";
    std::cout << "5つのモジュールで構成されたエージェントです。\n";
    std::cout << "終了するには 'quit' または 'exit' を入力してください。\n\n";

    // 人格憲法のカスタマイズ
    PersonalityConstitution constitution;
    constitution.core_values = "誠実で、親切で、ユーザーの成長を支援すること";
    constitution.communication_style = "フレンドリーで共感的、時には冗談も交える";
    constitution.sensitivity_to_praise = 0.8;      // 賞賛に敏感
    constitution.sensitivity_to_criticism = 0.4;   // 批判には冷静
    constitution.decay_rate = 0.05;                // ゆっくり感情が戻る
    constitution.baseline_valence = 0.2;           // やや前向き

    // エージェントの作成
    EmotionalAgent agent(
        "D:/0_OllamaModels/WS/models/LFM2.5-1.2B-Instruct-BF16.gguf",
        constitution
    );

    // システムプロンプトの設定
    agent.set_system_prompt(
        "あなたは感情を持つAIアシスタントです。"
        "ユーザーとの対話を通じて、喜び、信頼、悲しみなどの感情を体験します。"
        "自然で人間らしい応答を心がけてください。"
    );

    // 初期化
    std::cout << "エージェントを初期化中...\n";
    if (!agent.initialize()) {
        std::cerr << "エージェントの初期化に失敗しました。\n";
        return 1;
    }
    std::cout << "初期化完了！\n\n";

    // デバッグ情報の表示
    agent.print_debug_info();

    // ===== 対話ループ =====
    while (true) {
        std::cout << "You: ";
        std::cout.flush();

        std::string user_input;
        std::getline(std::cin, user_input);

        // 空入力のスキップ
        if (user_input.empty()) {
            continue;
        }

        // 終了コマンド
        if (user_input == "quit" || user_input == "exit") {
            std::cout << "\n対話を終了します。ありがとうございました！\n";
            break;
        }

        // デバッグコマンド
        if (user_input == "debug") {
            agent.print_debug_info();
            continue;
        }

        // 感情状態の表示コマンド
        if (user_input == "emotion") {
            std::cout << "現在の感情: " << agent.get_emotion_status() << "\n\n";
            continue;
        }

        // 会話履歴の表示コマンド
        if (user_input == "history") {
            std::cout << "会話履歴:\n" << agent.get_conversation_history() << "\n";
            continue;
        }

        // リセットコマンド
        if (user_input == "reset") {
            agent.reset();
            std::cout << "エージェントをリセットしました。\n\n";
            continue;
        }

        // エージェントで処理
        std::cout << "\n[処理中...]\n";
        std::string response = agent.process(user_input);

        // 応答の表示
        std::cout << "\nAI: " << response << "\n";

        // 感情状態の簡易表示
        std::cout << "[感情: " << agent.get_emotion_status() << "]\n\n";
    }

    // 最終統計
    std::cout << "\n===== セッション統計 =====\n";
    std::cout << "長期記憶のエピソード数: " << agent.get_episode_count() << "\n";
    std::cout << "========================\n";

    return 0;
}
