#include "EmotionalAgent.h"
#include "DialogFunctions.h"
#include "InputAnalyzer.h"
#include "MemoryController.h"
#include "Config.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>

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
        
        bool intent_ok = (result.intent == test.expected_intent);
        bool eval_ok = (result.evaluation_to_ai == test.expected_evaluation);
        bool sentiment_ok = (result.sentiment_score >= test.expected_sentiment_min &&
                            result.sentiment_score <= test.expected_sentiment_max);
        bool success = intent_ok && eval_ok && sentiment_ok;
        
        std::cout << "\n" << (success ? "[✓ PASS] " : "[✗ FAIL] ") << test.description 
                  << " (" << duration.count() << "ms)\n";
        std::cout << "  入力: \"" << test.input << "\"\n";
        std::cout << "  Intent:     " << result.intent;
        if (!intent_ok) std::cout << " (期待: " << test.expected_intent << ")";
        std::cout << "\n";
        std::cout << "  Evaluation: " << result.evaluation_to_ai;
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
        std::cout << "  Intent: " << result.intent << " (期待: question)\n";
        
        bool success = (result.intent == "question");
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
        std::cout << "  Intent: " << result.intent << " (期待: praise)\n";
        std::cout << "  Evaluation: " << result.evaluation_to_ai << " (期待: positive)\n";
        std::cout << "  Sentiment: " << std::fixed << std::setprecision(2) 
                  << result.sentiment_score << " (期待: > 0.5)\n";
        
        bool success = (result.intent == "praise" && 
                       result.evaluation_to_ai == "positive" && 
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
        std::cout << "  Intent: " << result.intent << " (期待: criticism)\n";
        std::cout << "  Evaluation: " << result.evaluation_to_ai << " (期待: negative)\n";
        std::cout << "  Sentiment: " << std::fixed << std::setprecision(2) 
                  << result.sentiment_score << " (期待: < 0)\n";
        
        bool success = (result.intent == "criticism" && 
                       result.evaluation_to_ai == "negative" && 
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
        std::cout << "  Intent: " << result.intent << " (期待: casual)\n";
        std::cout << "  Topic: " << result.topic << " (期待: 気候/天気)\n";
        
        bool success = (result.intent == "casual");
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
        std::cout << "  Intent: " << result.intent << " (期待: praise or casual)\n";
        std::cout << "  Sentiment: " << std::fixed << std::setprecision(2) 
                  << result.sentiment_score << " (期待: > 0.2)\n";
        
        bool success = ((result.intent == "praise" || result.intent == "casual") && 
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

int main(int argc, char** argv) {
    // コマンドライン引数のチェック
    bool debug_mode = false;
    
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
        // キーワードベースのみのテスト
        else if (arg == "--test-keyword") {
            run_analyzer_test_mode(false, "");  // キーワードベースのみ
            return 0;
        }
        // ヘルプ
        else if (arg == "--help" || arg == "-h") {
            std::cout << "感情駆動型AIエージェント\n\n";
            std::cout << "使用方法:\n";
            std::cout << "  LLMapp.exe                  - 通常の対話モード\n";
            std::cout << "  LLMapp.exe --debug          - デバッグモード（詳細ログ出力）\n";
            std::cout << "  LLMapp.exe --test-analyzer  - InputAnalyzer テスト (LLMモード)\n";
            std::cout << "  LLMapp.exe --test-keyword   - InputAnalyzer テスト (キーワードのみ)\n";
            std::cout << "  LLMapp.exe --help           - このヘルプを表示\n";
            return 0;
        }
    }
    
    // ===== 通常の対話モード =====
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
        DEFAULT_MODEL_PATH,
        constitution
    );

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
    
    // デバッグモードの設定
    if (debug_mode) {
        agent.set_debug_mode(true);
        std::cout << "\n※ デバッグモード: 全ての内部処理が表示されます\n\n";
    }

    // デバッグ情報の表示
    if (!debug_mode) {
        agent.print_debug_info();
    }

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
