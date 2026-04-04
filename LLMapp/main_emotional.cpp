#include "EmotionalAgent.h"
#include "DialogFunctions.h"
#include "InputAnalyzer.h"
#include "MemoryController.h"
#include "AgentStatePersistence.h"
#include "Config.h"
#include "ToolSetup.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>

namespace {
std::string trim_copy(const std::string& text) {
    auto is_ws = [](unsigned char c) { return std::isspace(c) != 0; };

    auto first = std::find_if_not(text.begin(), text.end(), is_ws);
    if (first == text.end()) {
        return "";
    }

    auto last = std::find_if_not(text.rbegin(), text.rend(), is_ws).base();
    return std::string(first, last);
}

std::string to_lower_copy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string normalize_command_token(const std::string& input) {
    std::string token = trim_copy(input);

    if (!token.empty() && token.front() == '/') {
        token.erase(0, 1);
    }

    if (token.size() >= 2) {
        const char front = token.front();
        const char back = token.back();
        const bool double_quoted = (front == '"' && back == '"');
        const bool single_quoted = (front == '\'' && back == '\'');
        if (double_quoted || single_quoted) {
            token = token.substr(1, token.size() - 2);
        }
    }

    token = trim_copy(token);
    token = to_lower_copy(token);

    auto to_alpha_only = [](const std::string& text) {
        std::string alpha;
        alpha.reserve(text.size());
        for (unsigned char c : text) {
            if (std::isalpha(c)) {
                alpha.push_back(static_cast<char>(std::tolower(c)));
            }
        }
        return alpha;
    };

    if (token == "exit" || token == "quit" || token == "debug" ||
        token == "emotion" || token == "history" || token == "reset" ||
        token == "prompt" || token == "help" || token == "commands" || token == "?") {
        return token;
    }

    if (token == "終了" || token == "ヘルプ" || token == "感情" ||
        token == "履歴" || token == "リセット" || token == "プロンプト") {
        if (token == "終了") return "exit";
        if (token == "ヘルプ") return "help";
        if (token == "感情") return "emotion";
        if (token == "履歴") return "history";
        if (token == "リセット") return "reset";
        if (token == "プロンプト") return "prompt";
    }

    const std::string alpha_only = to_alpha_only(token);
    if (alpha_only == "exit" || alpha_only == "quit" || alpha_only == "debug" ||
        alpha_only == "emotion" || alpha_only == "history" || alpha_only == "reset" ||
        alpha_only == "prompt" || alpha_only == "help" || alpha_only == "commands") {
        return alpha_only;
    }

    return token;
}

void print_interactive_command_help() {
    std::cout << "対話中コマンド:\n";
    std::cout << "  help | commands | ?           - このヘルプを表示\n";
    std::cout << "  debug                         - デバッグ情報表示\n";
    std::cout << "  emotion                       - 現在の感情状態を表示\n";
    std::cout << "  history                       - 会話履歴を表示\n";
    std::cout << "  reset                         - 状態をリセット\n";
    std::cout << "  prompt [tool|response|both] [text]\n";
    std::cout << "                                - プロンプトプレビュー（状態更新なし）\n";
    std::cout << "  quit | exit                   - 対話を終了\n";
    std::cout << "\nショート形式: /help, /prompt tool, /prompt response, /prompt both\n";
}

bool read_text_file(const std::string& file_path, std::string& out_content, std::string& out_error) {
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs.is_open()) {
        out_error = "ファイルを開けませんでした: " + file_path;
        return false;
    }

    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    if (!ifs.good() && !ifs.eof()) {
        out_error = "ファイル読み込み中にエラーが発生しました: " + file_path;
        return false;
    }

    out_content = buffer.str();

    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    if (out_content.size() >= 3 &&
        static_cast<unsigned char>(out_content[0]) == bom[0] &&
        static_cast<unsigned char>(out_content[1]) == bom[1] &&
        static_cast<unsigned char>(out_content[2]) == bom[2]) {
        out_content.erase(0, 3);
    }

    return true;
}

std::string current_local_time_text() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t current = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &current);
#else
    local_tm = *std::localtime(&current);
#endif

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string parent_dir_of_path(const std::string& file_path) {
    const auto pos = file_path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return ".";
    }
    return file_path.substr(0, pos);
}

std::string build_one_shot_log_file_path(const std::string& system_prompt_file_path) {
    const std::string parent_dir = parent_dir_of_path(system_prompt_file_path);
    if (parent_dir.empty() || parent_dir == ".") {
        return "_oneshot_runtime.log";
    }
    return parent_dir + "\\_oneshot_runtime.log";
}

std::string resolve_model_path_for_run() {
    const char* value = std::getenv("LLMAPP_MODEL_PATH");
    if (!value) {
        return DEFAULT_MODEL_PATH;
    }

    std::string model_path = trim_copy(std::string(value));
    if (model_path.empty()) {
        return DEFAULT_MODEL_PATH;
    }

    return model_path;
}

void append_one_shot_log_line(const std::string& log_file_path, const std::string& message) {
    std::ofstream ofs(log_file_path, std::ios::binary | std::ios::app);
    if (!ofs.is_open()) {
        return;
    }
    ofs << "[" << current_local_time_text() << "] " << message << "\n";
}
}

// テストケース構造体
struct TestCase {
    std::string input;
    std::string expected_intent;
    std::string expected_evaluation;
    double expected_sentiment_min;
    double expected_sentiment_max;
    std::string description;
};

// InputAnalyzerのテストモード
void run_analyzer_test_mode(bool use_llm, const std::string& model_path) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << " InputAnalyzer テストモード\n";
    std::cout << "========================================\n";
    std::cout << "モード: " << (use_llm ? "LLM + キーワード (ハイブリッド)" : "キーワードベースのみ") << "\n";
    
    InputAnalyzer analyzer;
    std::unique_ptr<LLMInference> llm;
    
    // LLMモードの初期化
    if (use_llm) {
        std::cout << "\nLLMを初期化中...\n";
        llm = std::make_unique<LLMInference>(
            model_path, 
            DEFAULT_GPU_LAYERS, 
            TEST_CONTEXT_SIZE, 
            TEST_N_PREDICT
        );
        
        if (!llm->initialize()) {
            std::cerr << "エラー: LLMの初期化に失敗しました\n";
            return;
        }
        
        analyzer.set_llm_inference(llm.get());
        analyzer.enable_llm_mode(true);
        std::cout << "LLM初期化完了！\n";
    }
    
    // テストケース定義
    std::vector<TestCase> test_cases = {
        // 基本テスト: 称賛系
        {"すごい！本当に助かりました！ありがとう！", 
         "praise", "positive", 0.6, 1.0, "基本的な称賛"},
        {"あなたの説明はとても分かりやすくて素晴らしいです", 
         "praise", "positive", 0.5, 1.0, "丁寧な称賛"},
        
        // 基本テスト: 批判系
        {"全然ダメじゃん。使えないなあ。", 
         "criticism", "negative", -1.0, -0.4, "強い批判"},
        {"間違ってますよ。もっとちゃんと答えてください。", 
         "criticism", "negative", -0.8, -0.2, "指摘と要求"},
        
        // 基本テスト: 質問系
        {"Pythonの辞書とリストの違いは何ですか？", 
         "question", "neutral", -0.2, 0.2, "技術的な質問"},
        {"どうやったらこのエラーを解決できる？", 
         "question", "neutral", -0.3, 0.2, "問題解決の質問"},
        
        // 基本テスト: 挨拶系
        {"こんにちは！今日もよろしくね！", 
         "greeting", "neutral", 0.2, 0.8, "明るい挨拶"},
        
        // 基本テスト: 雑談系
        {"今日は良い天気ですね", 
         "casual", "neutral", 0.0, 0.5, "天気の話題"},
        
        // 複雑なケース (LLMで精度向上)
        {"ありがとう。でも、これって本当に正確なの？", 
         "question", "neutral", 0.0, 0.4, "感謝+疑念 (LLM重要)"},
        {"悪くないけど、もう少し詳しく説明してほしかったな", 
         "criticism", "neutral", -0.5, 0.1, "混合感情 (LLM重要)"},
        {"へぇ、そうなんだ。すごいね", 
         "casual", "neutral", 0.0, 0.4, "※皮肉・相槌検出 (LLM)"},
    };
    
    // テスト実行
    std::cout << "\n--- テスト実行 ---\n";
    int passed = 0;
    int failed = 0;
    long long total_time = 0;
    
    for (const auto& test : test_cases) {
        auto start = std::chrono::high_resolution_clock::now();
        
        AnalyzedInput result = analyzer.analyze(test.input);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        total_time += duration.count();
        
        bool intent_ok = (intent_to_string(result.intent) == test.expected_intent);
        bool eval_ok = (evaluation_to_string(result.evaluation_to_ai) == test.expected_evaluation);
        bool sentiment_ok = (result.sentiment_score >= test.expected_sentiment_min &&
                            result.sentiment_score <= test.expected_sentiment_max);
        bool success = intent_ok && eval_ok && sentiment_ok;
        
        std::cout << "\n" << (success ? "[✓ PASS] " : "[✗ FAIL] ") << test.description 
                  << " (" << duration.count() << "ms)\n";
        std::cout << "  入力: \"" << test.input << "\"\n";
        std::cout << "  Intent:     " << intent_to_string(result.intent);
        if (!intent_ok) std::cout << " (期待: " << test.expected_intent << ")";
        std::cout << "\n";
        std::cout << "  Evaluation: " << evaluation_to_string(result.evaluation_to_ai);
        if (!eval_ok) std::cout << " (期待: " << test.expected_evaluation << ")";
        std::cout << "\n";
        std::cout << "  Sentiment:  " << std::fixed << std::setprecision(2) << result.sentiment_score;
        if (!sentiment_ok) {
            std::cout << " (期待: " << test.expected_sentiment_min 
                      << " ~ " << test.expected_sentiment_max << ")";
        }
        std::cout << "\n";
        
        if (success) passed++;
        else failed++;
    }
    
    // サマリー
    std::cout << "\n========================================\n";
    std::cout << " テスト結果サマリー\n";
    std::cout << "========================================\n";
    std::cout << "合計:   " << (passed + failed) << " テスト\n";
    std::cout << "合格:   " << passed << "\n";
    std::cout << "不合格: " << failed << "\n";
    std::cout << "合格率: " << std::fixed << std::setprecision(1) 
              << (100.0 * passed / (passed + failed)) << "%\n";
    std::cout << "平均処理時間: " << (total_time / test_cases.size()) << "ms\n";
    
    if (!use_llm) {
        std::cout << "\n[ヒント] LLMモードでテストする場合:\n";
        std::cout << "  LLMapp.exe --test-analyzer\n";
    }
    
    // 結果をファイルに保存
    auto now = std::chrono::system_clock::now();
    auto now_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &now_t);
    
    std::ostringstream filename;
    filename << "test_results_"
             << std::put_time(&tm_buf, "%Y%m%d_%H%M%S")
             << (use_llm ? "_llm" : "_keyword")
             << ".txt";
    
    std::ofstream outfile(filename.str());
    if (outfile.is_open()) {
        outfile << "========================================\n";
        outfile << " InputAnalyzer テスト結果\n";
        outfile << "========================================\n";
        outfile << "実行日時: " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "\n";
        outfile << "モード: " << (use_llm ? "LLM + キーワード (ハイブリッド)" : "キーワードベースのみ") << "\n";
        outfile << "\n========================================\n";
        outfile << " サマリー\n";
        outfile << "========================================\n";
        outfile << "合計:   " << (passed + failed) << " テスト\n";
        outfile << "合格:   " << passed << "\n";
        outfile << "不合格: " << failed << "\n";
        outfile << "合格率: " << std::fixed << std::setprecision(1) 
                << (100.0 * passed / (passed + failed)) << "%\n";
        outfile << "平均処理時間: " << (total_time / test_cases.size()) << "ms\n";
        outfile.close();
        
        std::cout << "\n[保存] テスト結果を " << filename.str() << " に保存しました\n";
    } else {
        std::cerr << "\n[エラー] テスト結果ファイルの保存に失敗しました\n";
    }
    
    // ===== 会話履歴を含むテスト =====
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << " 会話履歴付きテスト\n";
    std::cout << "========================================\n";
    std::cout << "文脈を考慮した入力分析のテストを実行します。\n\n";
    
    int context_passed = 0;
    int context_failed = 0;
    
    // テストケース1: 指示語の解釈
    {
        std::cout << "[テスト1] 指示語の解釈\n";
        std::deque<ConversationTurn> history;
        history.push_back(ConversationTurn("user", "Pythonの辞書について教えて"));
        history.push_back(ConversationTurn("assistant", "Pythonの辞書は、キーと値のペアを格納するデータ構造です。"));
        history.push_back(ConversationTurn("user", "リストとの違いは？"));
        history.push_back(ConversationTurn("assistant", "リストは順序付きのコレクションで、辞書はキーでアクセスします。"));
        
        std::cout << "  【会話履歴】\n";
        for (const auto& turn : history) {
            std::cout << "    " << turn.role << ": " << turn.content << "\n";
        }
        
        std::string test_input = "それはわかった。具体例を教えて";
        auto result = analyzer.analyze(test_input, &history);
        
        std::cout << "  入力: \"" << test_input << "\"\n";
        std::cout << "  Topic: " << result.topic << " (期待: Pythonの辞書関連)\n";
        std::cout << "  Intent: " << intent_to_string(result.intent) << " (期待: question)\n";
        
        bool success = (result.intent == Intent::QUESTION);
        std::cout << "  結果: " << (success ? "✓ PASS" : "✗ FAIL") << "\n\n";
        if (success) context_passed++; else context_failed++;
    }
    
    // テストケース2: 継続的な話題
    {
        std::cout << "[テスト2] 継続的な話題\n";
        std::deque<ConversationTurn> history;
        history.push_back(ConversationTurn("user", "機械学習について教えて"));
        history.push_back(ConversationTurn("assistant", "機械学習は、データからパターンを学習する技術です。"));
        history.push_back(ConversationTurn("user", "もっと詳しく"));
        history.push_back(ConversationTurn("assistant", "教師あり学習、教師なし学習、強化学習の3種類があります。"));
        
        std::cout << "  【会話履歴】\n";
        for (const auto& turn : history) {
            std::cout << "    " << turn.role << ": " << turn.content << "\n";
        }
        
        std::string test_input = "わかりやすい！ありがとう";
        auto result = analyzer.analyze(test_input, &history);
        
        std::cout << "  入力: \"" << test_input << "\"\n";
        std::cout << "  Intent: " << intent_to_string(result.intent) << " (期待: praise)\n";
        std::cout << "  Evaluation: " << evaluation_to_string(result.evaluation_to_ai) << " (期待: positive)\n";
        std::cout << "  Sentiment: " << std::fixed << std::setprecision(2) 
                  << result.sentiment_score << " (期待: > 0.5)\n";
        
        bool success = (result.intent == Intent::PRAISE && 
                   result.evaluation_to_ai == EvaluationToAI::POSITIVE && 
                       result.sentiment_score > 0.5);
        std::cout << "  結果: " << (success ? "✓ PASS" : "✗ FAIL") << "\n\n";
        if (success) context_passed++; else context_failed++;
    }
    
    // テストケース3: 前の回答への批判
    {
        std::cout << "[テスト3] 前の回答への批判\n";
        std::deque<ConversationTurn> history;
        history.push_back(ConversationTurn("user", "量子コンピュータって何？"));
        history.push_back(ConversationTurn("assistant", "量子力学の原理を使った新しいコンピュータです。"));
        
        std::cout << "  【会話履歴】\n";
        for (const auto& turn : history) {
            std::cout << "    " << turn.role << ": " << turn.content << "\n";
        }
        
        std::string test_input = "もっと具体的に説明してよ。そんな抽象的な説明じゃわからない";
        auto result = analyzer.analyze(test_input, &history);
        
        std::cout << "  入力: \"" << test_input << "\"\n";
        std::cout << "  Intent: " << intent_to_string(result.intent) << " (期待: criticism)\n";
        std::cout << "  Evaluation: " << evaluation_to_string(result.evaluation_to_ai) << " (期待: negative)\n";
        std::cout << "  Sentiment: " << std::fixed << std::setprecision(2) 
                  << result.sentiment_score << " (期待: < 0)\n";
        
        bool success = (result.intent == Intent::CRITICISM && 
                   result.evaluation_to_ai == EvaluationToAI::NEGATIVE && 
                       result.sentiment_score < 0);
        std::cout << "  結果: " << (success ? "✓ PASS" : "✗ FAIL") << "\n\n";
        if (success) context_passed++; else context_failed++;
    }
    
    // テストケース4: 話題の切り替え
    {
        std::cout << "[テスト4] 話題の切り替え\n";
        std::deque<ConversationTurn> history;
        history.push_back(ConversationTurn("user", "C++のポインタについて"));
        history.push_back(ConversationTurn("assistant", "ポインタはメモリアドレスを格納する変数です。"));
        history.push_back(ConversationTurn("user", "参照との違いは？"));
        history.push_back(ConversationTurn("assistant", "参照はエイリアスで、nullにできません。"));
        
        std::cout << "  【会話履歴】\n";
        for (const auto& turn : history) {
            std::cout << "    " << turn.role << ": " << turn.content << "\n";
        }
        
        std::string test_input = "ところで、今日は良い天気だね";
        auto result = analyzer.analyze(test_input, &history);
        
        std::cout << "  入力: \"" << test_input << "\"\n";
        std::cout << "  Intent: " << intent_to_string(result.intent) << " (期待: casual)\n";
        std::cout << "  Topic: " << result.topic << " (期待: 気候/天気)\n";
        
        bool success = (result.intent == Intent::CASUAL);
        std::cout << "  結果: " << (success ? "✓ PASS" : "✗ FAIL") << "\n\n";
        if (success) context_passed++; else context_failed++;
    }
    
    // テストケース5: 複雑な指示語チェーン
    {
        std::cout << "[テスト5] 複雑な指示語チェーン\n";
        std::deque<ConversationTurn> history;
        history.push_back(ConversationTurn("user", "メモリリークって何？"));
        history.push_back(ConversationTurn("assistant", "確保したメモリを解放し忘れることです。"));
        history.push_back(ConversationTurn("user", "それってどうやって防ぐの？"));
        history.push_back(ConversationTurn("assistant", "スマートポインタを使うと自動的に解放されます。"));
        
        std::cout << "  【会話履歴】\n";
        for (const auto& turn : history) {
            std::cout << "    " << turn.role << ": " << turn.content << "\n";
        }
        
        std::string test_input = "なるほど！それすごく便利そう";
        auto result = analyzer.analyze(test_input, &history);
        
        std::cout << "  入力: \"" << test_input << "\"\n";
        std::cout << "  Intent: " << intent_to_string(result.intent) << " (期待: praise or casual)\n";
        std::cout << "  Sentiment: " << std::fixed << std::setprecision(2) 
                  << result.sentiment_score << " (期待: > 0.2)\n";
        
        bool success = ((result.intent == Intent::PRAISE || result.intent == Intent::CASUAL) && 
                       result.sentiment_score > 0.2);
        std::cout << "  結果: " << (success ? "✓ PASS" : "✗ FAIL") << "\n\n";
        if (success) context_passed++; else context_failed++;
    }
    
    // 会話履歴付きテストのサマリー
    std::cout << "========================================\n";
    std::cout << " 会話履歴付きテスト サマリー\n";
    std::cout << "========================================\n";
    std::cout << "合計:   " << (context_passed + context_failed) << " テスト\n";
    std::cout << "合格:   " << context_passed << "\n";
    std::cout << "不合格: " << context_failed << "\n";
    std::cout << "合格率: " << std::fixed << std::setprecision(1) 
              << (100.0 * context_passed / (context_passed + context_failed)) << "%\n";
    
    std::cout << "\n";
}

// MemoryController / 永続化のテストモード
void run_memory_test_mode() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << " Memory / Persistence テストモード\n";
    std::cout << "========================================\n";

    int passed = 0;
    int failed = 0;

    auto report = [&](const std::string& title, bool ok, const std::string& detail = "") {
        std::cout << (ok ? "[✓ PASS] " : "[✗ FAIL] ") << title;
        if (!detail.empty()) {
            std::cout << " - " << detail;
        }
        std::cout << "\n";
        if (ok) {
            passed++;
        } else {
            failed++;
        }
    };

    // Test 1: 短期メモリFIFOと上限
    {
        MemoryController memory(3);
        memory.add_to_short_term("user", "A");
        memory.add_to_short_term("assistant", "B");
        memory.add_to_short_term("user", "C");
        memory.add_to_short_term("assistant", "D");

        const auto& history = memory.get_short_term_history();
        const bool ok = history.size() == 3 && history.front().content == "B" && history.back().content == "D";
        report("短期メモリFIFO・上限制御", ok, "期待: B,C,D が残る");
    }

    // Test 2: 要約上書き付き統合
    {
        MemoryController memory(10);
        memory.add_to_short_term("user", "Python辞書について知りたい");
        memory.add_to_short_term("assistant", "キーと値のペア構造です");
        memory.add_to_short_term("user", "具体例を教えて");
        memory.add_to_short_term("assistant", "例えば name: ritsu です");
        memory.add_to_short_term("user", "理解できた、ありがとう");

        const std::string summary = "ユーザーはPython辞書の概念と具体例を理解した。";
        memory.consolidate_memory("感情状態: 喜び（中程度） | 感情価: ポジティブ", {"Python", "辞書", "具体例"}, summary);

        const auto& episodes = memory.get_all_episodes();
        bool ok = !episodes.empty();
        if (ok) {
            ok = (episodes.front().summary == summary) &&
                 (episodes.front().emotional_tag == "positive") &&
                 (!episodes.front().keywords.empty());
        }
        report("要約上書き統合", ok, "LLM要約相当の文字列が保存されること");
    }

    // Test 3: 関連検索スコア
    {
        MemoryController memory(10);

        Episode ep1("C++のスマートポインタでメモリ管理を改善した。", "positive", 0.85);
        ep1.keywords = {"C++", "スマートポインタ", "メモリ管理"};
        memory.add_to_long_term(ep1);

        Episode ep2("天気の話をした。", "neutral", 0.20);
        ep2.keywords = {"天気", "雑談"};
        memory.add_to_long_term(ep2);

        auto results = memory.search_episodes({"C++", "ポインタ"}, 1);
        const bool ok = !results.empty() && results.front().summary.find("スマートポインタ") != std::string::npos;
        report("長期記憶検索", ok, "関連度の高いエピソードが上位に来ること");
    }

    // Test 4: 長期メモリ上限制御（100件）
    {
        MemoryController memory(10);
        for (int i = 0; i < 120; ++i) {
            Episode episode("episode-" + std::to_string(i), "neutral", 0.5 + (i % 10) * 0.01);
            episode.keywords = {"k" + std::to_string(i)};
            memory.add_to_long_term(episode);
        }

        const bool ok = memory.get_long_term_size() == 100;
        report("長期メモリ上限", ok, "100件を超えた分が切り詰められること");
    }

    // Test 5: 永続化ラウンドトリップ
    {
        AgentPersistentState state;
        state.constitution.core_values = "テスト用価値観";
        state.constitution.communication_style = "テスト口調";
        state.constitution.sensitivity_to_praise = 0.77;
        state.constitution.sensitivity_to_criticism = 0.33;
        state.constitution.decay_rate = 0.12;
        state.constitution.baseline_valence = 0.22;

        state.emotion_state.values[BasicEmotion::JOY] = 0.41;
        state.emotion_state.values[BasicEmotion::ANGER] = 0.05;
        state.emotion_state.overall_valence = 0.18;
        state.emotion_state.arousal = 0.44;

        state.short_term_limit = 12;
        state.short_term_memory.emplace_back("user", "保存テスト開始");
        state.short_term_memory.emplace_back("assistant", "了解、状態を保存します");

        Episode ep("保存機能のテストを実行した。", "positive", 0.70);
        ep.keywords = {"保存", "テスト"};
        state.long_term_memory.push_back(ep);

        state.system_prompt = "SYSTEM";
        state.tone_instruction = "TONE";
        state.max_episodes = 4;
        state.short_term_turns = 6;
        state.system_log_sections.push_back({"Test", "Memory persistence test"});
        state.debug_mode = true;

        const std::string temp_file = "memory_test_state.dat";
        bool ok = AgentStatePersistence::save_to_file(temp_file, state);

        AgentPersistentState loaded;
        if (ok) {
            ok = AgentStatePersistence::load_from_file(temp_file, loaded);
        }

        if (ok) {
            ok = (loaded.constitution.core_values == state.constitution.core_values) &&
                 (loaded.short_term_limit == state.short_term_limit) &&
                 (loaded.short_term_memory.size() == state.short_term_memory.size()) &&
                 (loaded.long_term_memory.size() == state.long_term_memory.size()) &&
                 (loaded.long_term_memory.front().summary == state.long_term_memory.front().summary) &&
                 (loaded.debug_mode == state.debug_mode);
        }

        std::remove(temp_file.c_str());
        report("永続化ラウンドトリップ", ok, "保存→読込で主要状態が一致すること");
    }

    std::cout << "\n========================================\n";
    std::cout << " Memory / Persistence テスト結果\n";
    std::cout << "========================================\n";
    std::cout << "合計:   " << (passed + failed) << " テスト\n";
    std::cout << "合格:   " << passed << "\n";
    std::cout << "不合格: " << failed << "\n";
    std::cout << "合格率: " << std::fixed << std::setprecision(1)
              << (100.0 * passed / (passed + failed)) << "%\n";
    std::cout << "\n";
}

// 統合テストモード（Analyzer + Memory/Persistence）
void run_integration_test_mode(const std::string& model_path) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << " 統合テストモード\n";
    std::cout << "========================================\n";
    std::cout << "InputAnalyzer と Memory/Persistence を連続実行します。\n\n";

    run_analyzer_test_mode(true, model_path);
    run_memory_test_mode();

    std::cout << "========================================\n";
    std::cout << " 統合テスト完了\n";
    std::cout << "========================================\n";
    std::cout << "\n";
}

int main(int argc, char** argv) {
    const std::string kAgentStateFile = "agent_state.dat";

    // コマンドライン引数のチェック
    bool debug_mode = false;
    bool run_once_with_system_prompt = false;
    bool one_shot_tool_phase_only = false;
    bool one_shot_response_phase_only = false;
    std::string one_shot_system_prompt_file;
    std::string one_shot_user_input;
    
    if (argc > 1) {
        std::string arg = argv[1];
        
        // デバッグモードのチェック
        if (arg == "--debug" || arg == "-d") {
            debug_mode = true;
            std::cout << "デバッグモードが有効化されました\n\n";
        }
        // テストモード
        else if (arg == "--test-analyzer" || arg == "--test") {
            run_analyzer_test_mode(true, DEFAULT_MODEL_PATH);  // LLMモード
            return 0;
        }
        // 統合テストモード
        else if (arg == "--test-all" || arg == "--test-integration") {
            run_integration_test_mode(DEFAULT_MODEL_PATH);
            return 0;
        }
        // 記憶・永続化テストモード
        else if (arg == "--test-memory") {
            run_memory_test_mode();
            return 0;
        }
        // キーワードベースのみのテスト
        else if (arg == "--test-keyword") {
            run_analyzer_test_mode(false, "");  // キーワードベースのみ
            return 0;
        }
        // システムプロンプトファイルで1回だけ推論して終了
        else if (arg == "--once-system-prompt") {
            if (argc < 3) {
                std::cerr << "エラー: --once-system-prompt にはファイルパスが必要です\n";
                std::cerr << "例: LLMapp.exe --once-system-prompt promptTest/TestPrompt.md\n";
                return 1;
            }

            run_once_with_system_prompt = true;
            one_shot_system_prompt_file = argv[2];

            if (argc > 3) {
                one_shot_user_input = argv[3];
            }
        }
        // システムプロンプトファイルでTool Phaseのみ1回実行して終了
        else if (arg == "--once-system-prompt-tool-phase") {
            if (argc < 3) {
                std::cerr << "エラー: --once-system-prompt-tool-phase にはファイルパスが必要です\n";
                std::cerr << "例: LLMapp.exe --once-system-prompt-tool-phase promptTest/Template_tool_minimal_v3.md\n";
                return 1;
            }

            run_once_with_system_prompt = true;
            one_shot_tool_phase_only = true;
            one_shot_system_prompt_file = argv[2];

            if (argc > 3) {
                one_shot_user_input = argv[3];
            }
        }
        // システムプロンプトファイルでResponse Phaseのみ1回実行して終了
        else if (arg == "--once-system-prompt-response-phase") {
            if (argc < 3) {
                std::cerr << "エラー: --once-system-prompt-response-phase にはファイルパスが必要です\n";
                std::cerr << "例: LLMapp.exe --once-system-prompt-response-phase promptTest/Template_response_minimal_v4.md\n";
                return 1;
            }

            run_once_with_system_prompt = true;
            one_shot_response_phase_only = true;
            one_shot_system_prompt_file = argv[2];

            if (argc > 3) {
                one_shot_user_input = argv[3];
            }
        }
        // ヘルプ
        else if (arg == "--help" || arg == "-h") {
            std::cout << "感情駆動型AIエージェント\n\n";
            std::cout << "使用方法:\n";
            std::cout << "  LLMapp.exe                  - 通常の対話モード\n";
            std::cout << "  LLMapp.exe --debug          - デバッグモード（詳細ログ出力）\n";
            std::cout << "  LLMapp.exe --test-analyzer  - InputAnalyzer テスト (LLMモード)\n";
            std::cout << "  LLMapp.exe --test-all       - 統合テスト (Analyzer + Memory)\n";
            std::cout << "  LLMapp.exe --test-memory    - Memory/Persistence テスト\n";
            std::cout << "  LLMapp.exe --test-keyword   - InputAnalyzer テスト (キーワードのみ)\n";
            std::cout << "  LLMapp.exe --once-system-prompt <file> [user_input] - 指定プロンプトで1回推論して終了\n";
            std::cout << "  LLMapp.exe --once-system-prompt-tool-phase <file> [user_input] - Tool Phaseのみ1回実行して終了\n";
            std::cout << "  LLMapp.exe --once-system-prompt-response-phase <file> [user_input] - Response Phaseのみ1回実行して終了\n";
            std::cout << "  LLMapp.exe --help           - このヘルプを表示\n";
            std::cout << "\n";
            print_interactive_command_help();
            return 0;
        }
    }

    if (run_once_with_system_prompt) {
        initialize_console();

        const std::string one_shot_log_file = build_one_shot_log_file_path(one_shot_system_prompt_file);
        {
            std::ofstream reset_log(one_shot_log_file, std::ios::binary | std::ios::trunc);
            if (reset_log.is_open()) {
                reset_log << "[" << current_local_time_text() << "] One-shot log initialized\n";
            }
        }

        auto log_one_shot = [&](const std::string& text) {
            append_one_shot_log_line(one_shot_log_file, text);
        };

        log_one_shot("start: system_prompt_file=" + one_shot_system_prompt_file);
        log_one_shot("start: user_input_length=" + std::to_string(one_shot_user_input.size()));
        std::cout << "[One-shot] 実行ログ: " << one_shot_log_file << "\n";

        std::string custom_system_prompt;
        std::string file_error;
        if (!read_text_file(one_shot_system_prompt_file, custom_system_prompt, file_error)) {
            log_one_shot("error: " + file_error);
            std::cerr << "エラー: " << file_error << "\n";
            return 1;
        }

        log_one_shot("loaded_system_prompt_bytes=" + std::to_string(custom_system_prompt.size()));

        if (trim_copy(custom_system_prompt).empty()) {
            log_one_shot("error: system prompt file is empty");
            std::cerr << "エラー: システムプロンプトファイルが空です: "
                      << one_shot_system_prompt_file << "\n";
            return 1;
        }

        PersonalityConstitution constitution;
        constitution.core_values = "誠実で、親切で、ユーザーの成長を支援すること";
        constitution.communication_style = "フレンドリーで共感的、時には冗談も交える";
        constitution.sensitivity_to_praise = 0.8;
        constitution.sensitivity_to_criticism = 0.4;
        constitution.decay_rate = 0.05;
        constitution.baseline_valence = 0.2;

        const std::string model_path_for_run = resolve_model_path_for_run();

        EmotionalAgent agent(
            model_path_for_run,
            constitution
        );

        log_one_shot("model_path=" + model_path_for_run);
        std::cout << "[One-shot] 使用モデル: " << model_path_for_run << "\n";

        register_default_tools(agent);
        agent.set_system_prompt(custom_system_prompt);

        std::cout << "[One-shot] システムプロンプトを読み込みました: "
                  << one_shot_system_prompt_file << "\n";
        if (one_shot_tool_phase_only) {
            std::cout << "[One-shot] Tool Phase専用モードで推論を実行します...\n\n";
        } else if (one_shot_response_phase_only) {
            std::cout << "[One-shot] Response Phase専用モードで推論を実行します...\n\n";
        } else {
            std::cout << "[One-shot] 推論を実行します...\n\n";
        }
        log_one_shot("agent_initialize_begin");

        if (!agent.initialize()) {
            log_one_shot("error: agent initialize failed");
            std::cerr << "エージェントの初期化に失敗しました。\n";
            return 1;
        }

        log_one_shot("agent_initialize_success");

        if (debug_mode) {
            agent.set_debug_mode(true);
            log_one_shot("debug_mode=enabled");
        }

    #if defined(_WIN32)
        _putenv_s("LLMAPP_ONE_SHOT_RUNTIME_LOG", one_shot_log_file.c_str());
    #else
        setenv("LLMAPP_ONE_SHOT_RUNTIME_LOG", one_shot_log_file.c_str(), 1);
    #endif
        log_one_shot("raw_output_logging=enabled");

        if (one_shot_tool_phase_only) {
    #if defined(_WIN32)
            _putenv_s("LLMAPP_TOOL_PHASE_ONLY", "1");
    #else
            setenv("LLMAPP_TOOL_PHASE_ONLY", "1", 1);
    #endif
            log_one_shot("tool_phase_only_mode=enabled");
        }

        if (one_shot_response_phase_only) {
    #if defined(_WIN32)
            _putenv_s("LLMAPP_RESPONSE_PHASE_ONLY", "1");
    #else
            setenv("LLMAPP_RESPONSE_PHASE_ONLY", "1", 1);
    #endif
            log_one_shot("response_phase_only_mode=enabled");
        }

        const auto infer_start = std::chrono::steady_clock::now();
        std::string response = agent.process(one_shot_user_input);
        const auto infer_end = std::chrono::steady_clock::now();

    #if defined(_WIN32)
        _putenv_s("LLMAPP_ONE_SHOT_RUNTIME_LOG", "");
    #else
        unsetenv("LLMAPP_ONE_SHOT_RUNTIME_LOG");
    #endif

        if (one_shot_tool_phase_only) {
    #if defined(_WIN32)
            _putenv_s("LLMAPP_TOOL_PHASE_ONLY", "");
    #else
            unsetenv("LLMAPP_TOOL_PHASE_ONLY");
    #endif
        }

        if (one_shot_response_phase_only) {
    #if defined(_WIN32)
            _putenv_s("LLMAPP_RESPONSE_PHASE_ONLY", "");
    #else
            unsetenv("LLMAPP_RESPONSE_PHASE_ONLY");
    #endif
        }

        const auto infer_ms = std::chrono::duration_cast<std::chrono::milliseconds>(infer_end - infer_start).count();

        log_one_shot("inference_done_ms=" + std::to_string(infer_ms));
        log_one_shot("response_length=" + std::to_string(response.size()));
        log_one_shot("response_text_begin");
        append_one_shot_log_line(one_shot_log_file, response);
        log_one_shot("response_text_end");

        std::cout << "AI: " << response << "\n";
        return 0;
    }
    
    // ===== 通常の対話モード =====
    initialize_console();

    std::cout << "===== 感情駆動型AIエージェント =====\n";
    std::cout << "5つのモジュールで構成されたエージェントです。\n";
    std::cout << "終了するには 'quit' または 'exit' を入力してください。\n\n";
    std::cout << "ヒント: 『数当てゲームをしよう』と話しかけるとゲームを開始できます。\n\n";

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
        DEFAULT_MODEL_PATH,
        constitution
    );

    // ツール登録は専用モジュールへ分離（mainの肥大化防止）
    register_default_tools(agent);

    // システムプロンプトの設定
    // 注: PromptOrchestratorに既にデフォルトのキャラクター設定（律）が組み込まれています
    // カスタマイズする場合のみ、以下のコメントを外してください
    /*
    agent.set_system_prompt(
        "あなたは感情を持つAIアシスタントです。"
        "ユーザーとの対話を通じて、喜び、信頼、悲しみなどの感情を体験します。"
        "自然で人間らしい応答を心がけてください。"
    );
    */

    // 初期化
    std::cout << "エージェントを初期化中...\n";
    if (!agent.initialize()) {
        std::cerr << "エージェントの初期化に失敗しました。\n";
        return 1;
    }
    std::cout << "初期化完了！\n\n";

    const bool loaded_state = agent.load_state_from_file(kAgentStateFile);
    if (loaded_state) {
        std::cout << "保存済み状態を復元しました: " << kAgentStateFile << "\n\n";
    } else {
        std::cout << "保存済み状態が見つからないため、新しいセッションを開始します。\n\n";
    }
    
    // デバッグモードの設定
    if (debug_mode) {
        agent.set_debug_mode(true);
        std::cout << "\n※ デバッグモード: 全ての内部処理が表示されます\n\n";
    }

    // デバッグ情報の表示
    if (!debug_mode) {
        agent.print_debug_info();
    }

    // 起動時の初回挨拶（履歴が空のときのみ）
    const std::string latest_history = agent.get_conversation_history(1);
    if (latest_history.empty()) {
        std::cout << "[起動メッセージ生成中...]\n";
        std::string startup_response = agent.process("");
        std::cout << "\nAI: " << startup_response << "\n";
        std::cout << "[感情: " << agent.get_emotion_status() << "]\n\n";
    } else {
        std::cout << "[保存済み履歴を復元したため、起動挨拶はスキップしました]\n\n";
        if (loaded_state) {
            std::cout << "復元した会話履歴:\n" << agent.get_conversation_history() << "\n\n";
        }
    }

    // ===== 対話ループ =====
    while (true) {
        std::cout << "You: ";
        std::cout.flush();

        std::string user_input;
        if (!std::getline(std::cin, user_input)) {
            std::cout << "\n入力ストリームが終了したため、対話を終了します。\n";
            break;
        }

        const std::string normalized_input = trim_copy(user_input);
        const std::string lowered_input = to_lower_copy(normalized_input);
        const std::string command_input = normalize_command_token(user_input);
        const bool is_slash_command = !normalized_input.empty() && normalized_input.front() == '/';

        // 空入力のスキップ
        if (normalized_input.empty()) {
            continue;
        }

        // 終了コマンド
        if (command_input == "quit" || command_input == "exit") {
            std::cout << "\n対話を終了します。ありがとうございました！\n";
            break;
        }

        // ヘルプコマンド
        if (command_input == "help" || command_input == "commands" || command_input == "?") {
            std::cout << "\n";
            print_interactive_command_help();
            std::cout << "\n";
            continue;
        }

        // デバッグコマンド
        if (command_input == "debug") {
            agent.print_debug_info();
            continue;
        }

        // 感情状態の表示コマンド
        if (command_input == "emotion") {
            std::cout << "現在の感情: " << agent.get_emotion_status() << "\n\n";
            continue;
        }

        // 会話履歴の表示コマンド
        if (command_input == "history") {
            std::cout << "会話履歴:\n" << agent.get_conversation_history() << "\n";
            continue;
        }

        // リセットコマンド
        if (command_input == "reset") {
            agent.reset();
            std::cout << "エージェントをリセットしました。\n\n";
            continue;
        }

        // プロンプト確認コマンド（状態更新なし）
        // 使い方:
        //   prompt
        //   prompt response [text]
        //   prompt tool [text]
        //   prompt both [text]
        //   /prompt tool [text]
        {
            std::string prompt_cmd = normalized_input;
            if (is_slash_command && !prompt_cmd.empty()) {
                prompt_cmd.erase(0, 1);
                prompt_cmd = trim_copy(prompt_cmd);
            }

            const std::string lowered_prompt_cmd = to_lower_copy(prompt_cmd);
            if (lowered_prompt_cmd == "prompt" || lowered_prompt_cmd.rfind("prompt ", 0) == 0) {
                std::istringstream iss(prompt_cmd);
                std::vector<std::string> tokens;
                std::string token;
                while (iss >> token) {
                    tokens.push_back(token);
                }

                bool preview_both = false;
                PromptOrchestrator::PromptPhase preview_phase = PromptOrchestrator::PromptPhase::Response;
                size_t text_start_index = 1;

                if (tokens.size() >= 2) {
                    const std::string arg = to_lower_copy(tokens[1]);
                    if (arg == "tool") {
                        preview_phase = PromptOrchestrator::PromptPhase::Tool;
                        text_start_index = 2;
                    } else if (arg == "response") {
                        preview_phase = PromptOrchestrator::PromptPhase::Response;
                        text_start_index = 2;
                    } else if (arg == "both" || arg == "all") {
                        preview_both = true;
                        text_start_index = 2;
                    } else {
                        preview_phase = PromptOrchestrator::PromptPhase::Response;
                        text_start_index = 1;
                    }
                }

                std::string preview_user_input;
                if (tokens.size() > text_start_index) {
                    std::ostringstream text_oss;
                    for (size_t i = text_start_index; i < tokens.size(); ++i) {
                        if (i > text_start_index) {
                            text_oss << " ";
                        }
                        text_oss << tokens[i];
                    }
                    preview_user_input = text_oss.str();
                }

                auto print_preview = [&](PromptOrchestrator::PromptPhase phase) {
                    const char* phase_name =
                        (phase == PromptOrchestrator::PromptPhase::Tool)
                        ? "Tool Phase"
                        : "Response Phase";

                    std::cout << "システムプロンプト（状態更新なし / " << phase_name << "）:\n";
                    if (!preview_user_input.empty()) {
                        std::cout << "[preview input] " << preview_user_input << "\n";
                    }
                    std::cout << "------------------------------------------------------------\n";
                    std::cout << agent.build_prompt_preview(preview_user_input, phase) << "\n";
                    std::cout << "------------------------------------------------------------\n\n";
                };

                if (preview_both) {
                    print_preview(PromptOrchestrator::PromptPhase::Tool);
                    print_preview(PromptOrchestrator::PromptPhase::Response);
                } else {
                    print_preview(preview_phase);
                }

                continue;
            }
        }

        // エージェントで処理
        std::cout << "\n[処理中...]\n";
        std::string response = agent.process(normalized_input);

        // 応答の表示
        std::cout << "\nAI: " << response << "\n";

        // 感情状態の簡易表示
        std::cout << "[感情: " << agent.get_emotion_status() << "]\n\n";
    }

    // 最終統計
    std::cout << "\n===== セッション統計 =====\n";
    std::cout << "長期記憶のエピソード数: " << agent.get_episode_count() << "\n";
    std::cout << "========================\n";

    if (agent.save_state_to_file(kAgentStateFile)) {
        std::cout << "状態を保存しました: " << kAgentStateFile << "\n";
    } else {
        std::cout << "状態の保存に失敗しました。\n";
    }

    return 0;
}
