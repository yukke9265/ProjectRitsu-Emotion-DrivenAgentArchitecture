#include "LLMInference.h"
#include "PromptManager.h"
#include <iostream>
#include <string>
#include <fstream>
#include <windows.h> // Win32 APIを使用するために追加
#include <locale>
#include <filesystem>

namespace fs = std::filesystem;

// ===== ファイルから初期入力を読み込む関数 =====
std::string load_initial_input_from_file(const std::string& directory = "TestTxt") {
    std::string initial_input;
    std::string search_dir = directory;

    // カレントディレクトリを取得してデバッグ出力
    std::cout << "[Debug] Current working directory: " << fs::current_path().string() << std::endl;
    std::cout << "[Debug] Searching for: " << search_dir << std::endl;

    // 指定されたディレクトリが存在するか確認
    if (!fs::exists(search_dir) || !fs::is_directory(search_dir)) {
        std::cout << "[Warning] " << search_dir << "/ ディレクトリが見つかりません。" << std::endl;
        
        // 親ディレクトリを検索してみる
        std::string parent_dir = "../" + search_dir;
        std::cout << "[Info] 親ディレクトリで検索: " << parent_dir << std::endl;
        
        if (fs::exists(parent_dir) && fs::is_directory(parent_dir)) {
            search_dir = parent_dir;
            std::cout << "[Info] 親ディレクトリで見つかりました: " << fs::absolute(search_dir).string() << std::endl;
        } else {
            return initial_input;
        }
    }

    // ディレクトリ内のファイルを探す
    for (const auto& entry : fs::directory_iterator(search_dir)) {
        if (entry.is_regular_file() && 
            (entry.path().extension() == ".txt" || entry.path().extension() == ".TXT")) {
            // 最初の .txt ファイルを読み込む
            std::ifstream file(entry.path());
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    if (!initial_input.empty()) {
                        initial_input += "\n";
                    }
                    initial_input += line;
                }
                file.close();
                std::cout << "[Info] ファイルから入力を読み込みました: " << entry.path().filename().string() << std::endl;
                std::cout << "[Content] " << initial_input << std::endl << std::endl;
                return initial_input;
            }
        }
    }

    // ファイルが見つからない場合
    std::cout << "[Warning] " << search_dir << "/ ディレクトリにテキストファイルが見つかりません。" << std::endl;
    return initial_input;
}

// ===== コンソール初期化関数 =====
void initialize_console() {
    // 1. コンソールの入出力を UTF-8 (65001) に設定
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 2. C++ の標準入出力ストリームに UTF-8 ロケールを適用
    try {
        std::locale::global(std::locale(".UTF8"));
    } catch (...) {
        std::locale::global(std::locale(""));
    }
    std::cin.imbue(std::locale());
    std::cout.imbue(std::locale());

    // 3. 標準入出力との同期を有効に保つ
    std::ios::sync_with_stdio(true);
}

// ===== LLM初期化関数 =====
bool create_and_initialize_llm(LLMInference& llm) {
    if (!llm.initialize()) {
        std::cerr << "[ERROR] Initialization failed: " << llm.get_last_error() << std::endl;
        return false;
    }
    return true;
}

// ===== PromptManager作成・設定関数 =====
void create_prompt_managers(PromptManager& pmA, PromptManager& pmB) {
    // PromptManagerAの設定（ユーザー入力を分析・要約）
    pmA.set_system("ユーザーの入力を分析しやすくするため、過不足なく要約してください。");

    // PromptManagerBの設定（プロンプトAの出力をさらに詳しく説明）
    pmB.set_system("与えられたテキストをさらに詳しく分析し、ユーザーに分かりやすく提示してください。");
}

// ===== 文字列トリム関数 =====
std::string trim_string(const std::string& str) {
    std::string trimmed = str;
    size_t end = trimmed.find_last_not_of(" \n\r\t");
    if (end != std::string::npos) {
        trimmed = trimmed.substr(0, end + 1);
    }
    return trimmed;
}

// ===== 2段階推論処理関数 =====
bool process_two_stage_inference(
    LLMInference& llm,
    PromptManager& pmA,
    PromptManager& pmB,
    const std::string& user_input,
    std::string& final_output
) {
    // ===== ステップ1: プロンプトAでユーザー入力を処理 =====
    std::string promptA = pmA.build_final(user_input);
    std::cout << "[Processing A] " << std::flush;
    std::string resultA = llm.infer(promptA);

    if (resultA.empty()) {
        std::cerr << "[ERROR] プロンプトA処理エラー: " << llm.get_last_error() << std::endl;
        return false;
    }

    std::string trimmed_resultA = trim_string(resultA);
    std::cout << "Done" << std::endl;

    // ===== ステップ2: プロンプトBでプロンプトAの出力を処理 =====
    std::string promptB = pmB.build_final(trimmed_resultA);
    std::cout << "[Processing B] " << std::flush;
    std::string resultB = llm.infer(promptB);

    if (resultB.empty()) {
        std::cerr << "[ERROR] プロンプトB処理エラー: " << llm.get_last_error() << std::endl;
        return false;
    }

    std::string trimmed_resultB = trim_string(resultB);
    std::cout << "Done" << std::endl;

    // ===== 履歴に追加 =====
    pmA.add_to_history("user", user_input);
    pmA.add_to_history("assistant", trimmed_resultA);

    pmB.add_to_history("user", trimmed_resultA);
    pmB.add_to_history("assistant", trimmed_resultB);

    // 最終出力を設定
    final_output = trimmed_resultB;
    return true;
}

// ===== ユーザー入力取得関数 =====
std::string get_user_input(bool& first_iteration, const std::string& initial_input) {
    std::string user_input;
    
    if (first_iteration && !initial_input.empty()) {
        // ファイルから読み込んだ内容を使用
        user_input = initial_input;
        std::cout << "You: " << user_input << std::endl;
        first_iteration = false;
    } else {
        // コンソールからの入力を受け取り
        std::cout << "You: ";
        std::cout.flush();
        std::getline(std::cin, user_input);
        first_iteration = false;
    }
    
    return user_input;
}

// ===== 対話ループ関数 =====
void run_interactive_dialog(
    LLMInference& llm,
    PromptManager& pmA,
    PromptManager& pmB,
    const std::string& initial_input,
    const std::string& system_prompt_a = "ユーザーの入力を分析しやすくするため、過不足なく要約してください。",
    const std::string& system_prompt_b = "与えられたテキストをさらに詳しく分析し、ユーザーに分かりやすく提示してください。"
) {
    // システムプロンプトを設定
    pmA.clear_system();
    pmA.set_system(system_prompt_a);
    pmB.clear_system();
    pmB.set_system(system_prompt_b);

    std::cout << "=== 対話型LLM推論システム（2段階処理） ===" << std::endl;
    std::cout << "ユーザー入力 → プロンプトA（要約）→ プロンプトB（説明）→ 出力" << std::endl;
    std::cout << "終了するには 'exit' または 'quit' を入力してください。" << std::endl << std::endl;

    bool first_iteration = true;

    while (true) {
        // ユーザー入力を取得
        std::string user_input = get_user_input(first_iteration, initial_input);

        // 終了コマンドのチェック
        if (user_input == "exit" || user_input == "quit") {
            std::cout << "プログラムを終了します。" << std::endl;
            break;
        }

        // 空入力のスキップ
        if (user_input.empty()) {
            continue;
        }

        // 2段階推論を実行
        std::string final_output;
        if (process_two_stage_inference(llm, pmA, pmB, user_input, final_output)) {
            std::cout << "Assistant: " << final_output << std::endl;
        }

        std::cout << std::endl;
    }
}

// ===== 交互対話ループ関数（A → B → A → B ... で繰り返す）=====
void run_alternating_dialog(
    LLMInference& llm,
    PromptManager& pmA,
    PromptManager& pmB,
    const std::string& initial_input,
    int num_iterations,
    const std::string& system_prompt_a = "与えられた内容を分析して、重要なポイントを短く要約してください。",
    const std::string& system_prompt_b = "与えられた内容についてさらに深く掘り下げた考察を提供してください。"
) {
    // システムプロンプトを設定
    pmA.clear_system();
    pmA.clear_history();
    pmA.set_system(system_prompt_a);
    
    pmB.clear_system();
    pmB.clear_history();
    pmB.set_system(system_prompt_b);

    std::cout << "=== 交互対話型LLM推論システム ===" << std::endl;
    std::cout << "話題提供 → プロンプトA → プロンプトB → プロンプトA → ... (繰り返し)" << std::endl;
    std::cout << "回数: " << num_iterations << " 回のサイクル" << std::endl;
    std::cout << "終了するには 'exit' または 'quit' を入力してください。" << std::endl << std::endl;

    bool first_iteration = true;
    bool should_exit = false;

    // ===== 複数サイクルを for ループで実行 =====
    for (int cycle = 0; cycle < num_iterations && !should_exit; ++cycle) {
        std::string topic_input;

        // ===== 話題提供を取得 =====
        if (first_iteration && !initial_input.empty()) {
            // ファイルから読み込んだ内容を使用
            topic_input = initial_input;
            std::cout << "Topic: " << topic_input << std::endl;
            first_iteration = false;
        } else if (cycle > 0) {
            // 2回目以降はコンソールから新しい話題を入力
            std::cout << std::endl << "Next Topic (or 'exit' to quit): ";
            std::cout.flush();
            std::getline(std::cin, topic_input);
        }

        // 終了コマンドのチェック
        if (topic_input == "exit" || topic_input == "quit") {
            std::cout << "プログラムを終了します。" << std::endl;
            should_exit = true;
            break;
        }

        // 空入力のスキップ
        if (topic_input.empty() && cycle == 0) {
            continue;
        }

        // ===== サイクル開始 =====
        std::cout << "\n--- Cycle " << (cycle + 1) << " / " << num_iterations << " ---" << std::endl;

        // ===== ステップ1: プロンプトAで処理 =====
        std::string promptA = pmA.build_final(topic_input);
        std::cout << "[Processing A] " << std::flush;
        std::string resultA = llm.infer(promptA);

        if (resultA.empty()) {
            std::cerr << "[ERROR] プロンプトA処理エラー: " << llm.get_last_error() << std::endl;
            should_exit = true;
            break;
        }

        std::string trimmed_resultA = trim_string(resultA);
        std::cout << "Done" << std::endl;
        std::cout << "A: " << trimmed_resultA << std::endl;

        pmA.add_to_history("user", topic_input);
        pmA.add_to_history("assistant", trimmed_resultA);

        // ===== ステップ2: プロンプトBで処理 =====
        std::string promptB = pmB.build_final(trimmed_resultA);
        std::cout << "[Processing B] " << std::flush;
        std::string resultB = llm.infer(promptB);

        if (resultB.empty()) {
            std::cerr << "[ERROR] プロンプトB処理エラー: " << llm.get_last_error() << std::endl;
            should_exit = true;
            break;
        }

        std::string trimmed_resultB = trim_string(resultB);
        std::cout << "Done" << std::endl;
        std::cout << "B: " << trimmed_resultB << std::endl;

        pmB.add_to_history("user", trimmed_resultA);
        pmB.add_to_history("assistant", trimmed_resultB);

        // ===== ステップ3: プロンプトAで再度処理（Bの出力に対して）=====
        std::string promptA2 = pmA.build_final(trimmed_resultB);
        std::cout << "[Processing A] " << std::flush;
        std::string resultA2 = llm.infer(promptA2);

        if (resultA2.empty()) {
            std::cerr << "[ERROR] プロンプトA処理エラー: " << llm.get_last_error() << std::endl;
            should_exit = true;
            break;
        }

        std::string trimmed_resultA2 = trim_string(resultA2);
        std::cout << "Done" << std::endl;
        std::cout << "A: " << trimmed_resultA2 << std::endl;

        pmA.add_to_history("user", trimmed_resultB);
        pmA.add_to_history("assistant", trimmed_resultA2);

        // ===== ステップ4: プロンプトBで再度処理（Aの出力に対して）=====
        std::string promptB2 = pmB.build_final(trimmed_resultA2);
        std::cout << "[Processing B] " << std::flush;
        std::string resultB2 = llm.infer(promptB2);

        if (resultB2.empty()) {
            std::cerr << "[ERROR] プロンプトB処理エラー: " << llm.get_last_error() << std::endl;
            should_exit = true;
            break;
        }

        std::string trimmed_resultB2 = trim_string(resultB2);
        std::cout << "Done" << std::endl;
        std::cout << "B: " << trimmed_resultB2 << std::endl;

        pmB.add_to_history("user", trimmed_resultA2);
        pmB.add_to_history("assistant", trimmed_resultB2);

        std::cout << std::endl;
    }

    std::cout << "\n=== 対話が完了しました ===" << std::endl;
}

// ===== 自動対話ループ関数（LLM同士が自動的に会話を続ける） - シンプル版（2ステップ）=====
void run_autonomous_alternating_dialog(
    LLMInference& llm,
    PromptManager& pmA,
    PromptManager& pmB,
    const std::string& initial_input,
    int num_iterations,
    const std::string& system_prompt_a = "与えられた内容を分析して、重要なポイントを短く要約してください。",
    const std::string& system_prompt_b = "与えられた内容についてさらに深く掘り下げた考察を提供してください。"
) {
    // システムプロンプトを設定
    pmA.clear_system();
    pmA.clear_history();
    pmA.set_system(system_prompt_a);
    
    pmB.clear_system();
    pmB.clear_history();
    pmB.set_system(system_prompt_b);

    std::cout << "=== 自動対話型LLM推論システム（シンプル版） ===" << std::endl;
    std::cout << "初期トピック → A → B → A → B → ... （LLM同士が自動的に会話） " << std::endl;
    std::cout << "サイクル数: " << num_iterations << " 回（1サイクル = A → B）" << std::endl << std::endl;

    // ===== 初期トピックの取得 =====
    std::string current_topic = initial_input;
    
    if (current_topic.empty()) {
        std::cout << "Initial Topic: ";
        std::cout.flush();
        std::getline(std::cin, current_topic);
        
        if (current_topic == "exit" || current_topic == "quit") {
            std::cout << "プログラムを終了します。" << std::endl;
            return;
        }
    } else {
        std::cout << "Initial Topic: " << current_topic << std::endl;
    }

    std::cout << std::endl;

    // ===== 複数サイクルを for ループで自動実行 =====
    for (int cycle = 0; cycle < num_iterations; ++cycle) {
        std::cout << "\n--- Cycle " << (cycle + 1) << " / " << num_iterations << " ---" << std::endl;

        // ===== ステップ1: プロンプトAで処理 =====
        std::string promptA = pmA.build_final(current_topic);
        std::cout << "[Processing A] " << std::flush;
        std::string resultA = llm.infer(promptA);

        if (resultA.empty()) {
            std::cerr << "[ERROR] プロンプトA処理エラー: " << llm.get_last_error() << std::endl;
            break;
        }

        std::string trimmed_resultA = trim_string(resultA);
        std::cout << "Done" << std::endl;
        std::cout << "A: " << trimmed_resultA << std::endl;

        pmA.add_to_history("user", current_topic);
        pmA.add_to_history("assistant", trimmed_resultA);

        // ===== ステップ2: プロンプトBで処理 =====
        std::string promptB = pmB.build_final(trimmed_resultA);
        std::cout << "[Processing B] " << std::flush;
        std::string resultB = llm.infer(promptB);

        if (resultB.empty()) {
            std::cerr << "[ERROR] プロンプトB処理エラー: " << llm.get_last_error() << std::endl;
            break;
        }

        std::string trimmed_resultB = trim_string(resultB);
        std::cout << "Done" << std::endl;
        std::cout << "B: " << trimmed_resultB << std::endl;

        pmB.add_to_history("user", trimmed_resultA);
        pmB.add_to_history("assistant", trimmed_resultB);

        // ===== 会話履歴を制限（直近2ターンのみを保持）=====
        pmA.limit_history(5);
        pmB.limit_history(5);

        // ===== 次のサイクルの入力を B の出力に設定 =====
        current_topic = trimmed_resultB;

        std::cout << std::endl;
    }

    std::cout << "\n=== " << num_iterations << " サイクルの自動対話が完了しました ===" << std::endl;
    std::cout << "\n【最終出力】" << std::endl;
    std::cout << current_topic << std::endl;
}

int main(int argc, char** argv) {
    // ===== 初期化 =====
    initialize_console();

    // ===== リソース取得 =====
    std::string initial_input = load_initial_input_from_file("TestTxt");
    
    if (initial_input.empty()) {
        std::cout << "対話モードで開始します。" << std::endl << std::endl;
    }

    // LLMの作成と初期化
    LLMInference llm(
        "D:/0_OllamaModels/WS/models/LFM2.5-1.2B-Instruct-BF16.gguf",
        99,    // GPU layers
        8192,  // context size
        -1     // n_predict
    );

    if (!create_and_initialize_llm(llm)) {
        return 1;
    }

    // PromptManagerの作成（システムプロンプトは各対話ループで設定）
    PromptManager pmA(2048);
    PromptManager pmB(2048);

    // ===== ビジネスロジック =====
    // 自然な会話向けのシステムプロンプトを定義
    std::string system_prompt_a = 
        "あなたはAIの発展について前向きで推進的な視点を持つ専門家です。\n"
        "相手の意見に対して、同意しつつも新しい観点や利点を加えてください。\n"
        "自然で流暢な日本語で、専門的かつ親しみやすく答えてください。";

    std::string system_prompt_b = 
        "あなたはAIの発展について批判的で検討的な視点を持つ専門家です。\n"
        "相手の意見の妥当性を評価し、見落としている課題やリスクを指摘してください。\n"
        "建設的で、相手を尊重した口調で回答してください。";

    // 自動対話ループ（LLM同士が自然な会話を続ける、5サイクル）
    run_autonomous_alternating_dialog(
        llm, pmA, pmB, initial_input, 5,
        system_prompt_a, system_prompt_b
    );

    return 0;
}