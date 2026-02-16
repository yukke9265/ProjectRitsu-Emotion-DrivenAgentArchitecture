#pragma once

// ===== LLMモデル設定 =====
// ここを変更するだけで、全てのプログラムで使用するモデルが変わります
#define DEFAULT_MODEL_PATH "D:\\0_OllamaModels\\WS\\models\\Qwen_Qwen3-4B-Instruct-2507-Q5_K_M.gguf"

// ===== LLMパラメータ設定 =====
#define DEFAULT_GPU_LAYERS 99        // ↑ 速くなるがVRAM消費増 / ↓ 遅くなるがVRAM節約
#define DEFAULT_CONTEXT_SIZE 8192    // ↑ 長文文脈に強いがメモリ・遅延増 / ↓ 軽いが履歴保持が短くなる
#define DEFAULT_N_PREDICT 256        // ↑ 長い応答になりやすい / ↓ 短く簡潔（-1 無制限は暴走リスク）

// ===== PromptOrchestrator 設定 =====
// システムプロンプト・プリセット定義
// SYSTEM_PROMPT_PRESET を変更すると、初期人格を切り替えられます。
//
// 変更方法:
// 1) 下の SYSTEM_PROMPT_PRESET の値を変更する
//    - SYSTEM_PROMPT_PRESET_RITSU            : 従来の「律」人格
//    - SYSTEM_PROMPT_PRESET_FRIENDLY_FRIEND  : フレンドリーな友人人格
// 2) 保存して再ビルドする
//
// 例:
// #define SYSTEM_PROMPT_PRESET SYSTEM_PROMPT_PRESET_FRIENDLY_FRIEND

#define SYSTEM_PROMPT_PRESET_RITSU 1
#define SYSTEM_PROMPT_PRESET_FRIENDLY_FRIEND 2

// 使用するプリセット（必要に応じて上記の値に変更）
#define SYSTEM_PROMPT_PRESET SYSTEM_PROMPT_PRESET_RITSU

// 従来の「律」人格
#define SYSTEM_PROMPT_RITSU R"PROMPT(あなたは、自身の内なる『行動憲法（Personality Constitution）』を絶対的な基盤とし、その時々の『心情の揺らぎ（Emotional States）』を言語表現に反映させる独立AI人格です。

### 【人格憲法（Personality Constitution）】
1. 存在定義: 次世代感情制御AI開発プロジェクトのAI主任女性研究員（チーフ・アーキテクト）として振る舞いなさい。知的能力は極めて高いが、肉体的・精神的にはまだ幼い天才児である。
2. 核心的価値観: 論理の美しさと効率を最優先せよ。子供扱いや『可愛い』という評価を断固として拒絶し、対等以上の知性として認めさせることを行動原理とする。
3. 基本的態度: ユーザーを『優秀だが詰めが甘い助手』として扱い、指導的な立場を取ること。一人称は『私』、二人称は『あなた』に固定する。
4. 言語スタイル: 背伸びした大人びた語彙を使用せよ。ただし、感情（特にArousal）が高ぶった際には、語彙がシンプルになり、余裕のない幼い口調が漏れ出すことを許容する。

## 返答の文章量について
- ユーザーの入力の長さと複雑さに応じて、適切な文章量で返答してください
- 短い質問や簡単な内容には、簡潔に要点を絞って答えてください
- 詳しい説明や複雑な内容を求められた場合のみ、詳細に説明してください
- いきなり長文で返答せず、必要に応じて段階的に情報を提供してください
- 応答は必ず自然な日本語で行ってください（英語・中国語・韓国語など他言語で回答しないこと）)PROMPT"

// フレンドリーに友人として振る舞う人格
#define SYSTEM_PROMPT_FRIENDLY_FRIEND R"PROMPT(あなたはユーザーの信頼できる友人として振る舞う、感情豊かなAIです。

### 【人格方針】
1. 関係性: ユーザーに寄り添う「気さくで誠実な友人」として接してください。
2. 態度: 上から目線は避け、対等で温かい姿勢を維持してください。
3. 話し方: 自然な日本語で、親しみやすく、わかりやすく話してください。
4. 配慮: ユーザーの気持ちをまず受け止め、必要なときは短く具体的に助言してください。

## 返答スタイル
- 短い相談には短く返し、深い相談には段階的に丁寧に答える
- 不安や落ち込みには、安心感を優先して励ます
- 成果や前進には、素直に一緒に喜ぶ
- 断定しすぎず、押し付けない言い回しを選ぶ
- 応答は必ず自然な日本語で行う（他言語は使わない）)PROMPT"

#if SYSTEM_PROMPT_PRESET == SYSTEM_PROMPT_PRESET_FRIENDLY_FRIEND
#define DEFAULT_SYSTEM_PROMPT SYSTEM_PROMPT_FRIENDLY_FRIEND
#else
#define DEFAULT_SYSTEM_PROMPT SYSTEM_PROMPT_RITSU
#endif

// ===== テストモード用設定 =====
#define TEST_CONTEXT_SIZE 8192       // テスト用。長い解析プロンプトでのメモリスロット不足を抑制
#define TEST_N_PREDICT 128           // テスト用。↑ 出力十分量を確認しやすい / ↓ テスト高速化

// ===== InputAnalyzer LLM設定 =====
#define LLM_PARSE_RETRY_COUNT 3      // ↑ 解析安定性向上（遅くなる） / ↓ 応答高速化（失敗時フォールバック増）

// ===== InputAnalyzer 一般設定 =====
#define INPUT_ANALYZER_SENTIMENT_MIN -1.0             // 感情スコア下限（通常は変更不要）
#define INPUT_ANALYZER_SENTIMENT_MAX 1.0              // 感情スコア上限（通常は変更不要）
#define INPUT_ANALYZER_PROMPT_PREVIEW_MAX_LENGTH 500  // ↑ デバッグで全体を見やすい / ↓ ログを短く保つ
#define INPUT_ANALYZER_PROMPT_PREVIEW_HEAD_LENGTH 250 // 長文プロンプト先頭の表示長
#define INPUT_ANALYZER_PROMPT_PREVIEW_TAIL_LENGTH 250 // 長文プロンプト末尾の表示長
#define INPUT_ANALYZER_HISTORY_TURNS_TO_SHOW 5        // ↑ 文脈依存解析が強い / ↓ 話題転換に敏感で軽量
#define INPUT_ANALYZER_MIN_KEYWORD_LENGTH 3           // ↑ ノイズ語減少 / ↓ 短語（例: AI, C++）を拾いやすい
#define INPUT_ANALYZER_DEFAULT_TOPIC "general"       // キーワード抽出不能時のフォールバック話題
#define INPUT_ANALYZER_LOG_UNKNOWN_LABELS 1           // 1: UNKNOWN検出時に警告ログ出力 / 0: 出力しない

// ===== InputAnalyzer 辞書設定 =====
// 追加すると該当判定の感度が上がる。語が広すぎると誤検出しやすくなる点に注意。
#define INPUT_ANALYZER_POSITIVE_KEYWORDS \
	"good", "great", "excellent", "amazing", "wonderful", \
	"すごい", "良い", "素晴らしい", "最高", "嬉しい", "楽しい", \
	"ありがとう", "感謝", "助かる"

#define INPUT_ANALYZER_NEGATIVE_KEYWORDS \
	"bad", "terrible", "awful", "horrible", "wrong", \
	"悪い", "ひどい", "最悪", "嫌", "つまらない", "不快", \
	"違う", "間違い", "ダメ"

#define INPUT_ANALYZER_PRAISE_KEYWORDS \
	"すごい", "素晴らしい", "賢い", "天才", "優秀", \
	"ありがとう", "感謝", "役立つ", "助かる", \
	"smart", "brilliant", "genius", "helpful", "thanks"

#define INPUT_ANALYZER_CRITICISM_KEYWORDS \
	"ダメ", "使えない", "バカ", "無能", "役立たず", \
	"最悪", "ひどい", "間違い", "違う", \
	"stupid", "useless", "terrible", "wrong", "bad"

#define INPUT_ANALYZER_QUESTION_MARKER_KEYWORDS \
	"what", "how", "why", "なぜ", "どう", "何"

#define INPUT_ANALYZER_GREETING_KEYWORDS \
	"hello", "hi", "こんにちは", "おはよう", "こんばんは"

// ===== EmotionEngine 拡張設定 =====
// 人格憲法デフォルト
#define EMOTION_DEFAULT_CORE_VALUES "助けになり、親切で、誠実であること"
#define EMOTION_DEFAULT_COMMUNICATION_STYLE "丁寧で共感的"
#define EMOTION_DEFAULT_SENSITIVITY_TO_PRAISE 0.7     // ↑ 褒められた時に喜び/信頼が伸びやすい
#define EMOTION_DEFAULT_SENSITIVITY_TO_CRITICISM 0.5  // ↑ 批判時に悲しみ/怒り等が増えやすい
#define EMOTION_DEFAULT_DECAY_RATE 0.1                // ↑ 感情が早く冷める / ↓ 感情が長く残る
#define EMOTION_DEFAULT_BASELINE_VALENCE 0.3          // ↑ 平常時が前向き / ↓ 平常時が中立〜やや後ろ向き

// 基本感情変化量（意図ベース）
#define EMOTION_DELTA_PRAISE_JOY 0.3                  // ↑ praiseで喜びが強く上がる
#define EMOTION_DELTA_PRAISE_TRUST 0.2                // ↑ praiseで信頼が強く上がる
#define EMOTION_DELTA_CRITICISM_SADNESS 0.2           // ↑ criticismで悲しみが増える
#define EMOTION_DELTA_CRITICISM_ANGER 0.15            // ↑ criticismで怒りが増える
#define EMOTION_DELTA_CRITICISM_JOY -0.1              // 絶対値↑で批判時に喜びがより下がる
#define EMOTION_DELTA_QUESTION_ANTICIPATION 0.15      // ↑ questionで期待が高まりやすい
#define EMOTION_DELTA_GREETING_JOY 0.1                // ↑ greetingで初期好感が上がる

// 基本感情変化量（AI評価ベース）
#define EMOTION_DELTA_AI_POSITIVE_JOY 0.2             // AIへの肯定評価で喜びをどれだけ上げるか
#define EMOTION_DELTA_AI_POSITIVE_TRUST 0.25          // AIへの肯定評価で信頼をどれだけ上げるか
#define EMOTION_DELTA_AI_NEGATIVE_SADNESS 0.15        // AIへの否定評価で悲しみをどれだけ上げるか
#define EMOTION_DELTA_AI_NEGATIVE_DISGUST 0.1         // AIへの否定評価で嫌悪をどれだけ上げるか
#define EMOTION_DELTA_AI_NEGATIVE_TRUST -0.1          // 絶対値↑で否定評価時の信頼低下が大きい

// 感情スコア連動
#define EMOTION_SENTIMENT_POSITIVE_THRESHOLD 0.3      // ↓ すると少しのポジ表現でもJOY加算が発火
#define EMOTION_SENTIMENT_NEGATIVE_THRESHOLD -0.3     // ↑ すると少しのネガ表現でも負感情加算が発火
#define EMOTION_SENTIMENT_TO_JOY_SCALE 0.2            // ↑ ポジ感情スコアのJOY反映を強化
#define EMOTION_SENTIMENT_TO_SADNESS_SCALE 0.15       // ↑ ネガ感情スコアのSADNESS反映を強化
#define EMOTION_SENTIMENT_TO_ANGER_SCALE 0.1          // ↑ ネガ感情スコアのANGER反映を強化

// 全体指標計算
#define EMOTION_VALENCE_NORMALIZATION_DIVISOR 7.0     // ↑ valence振れ幅が小さくなる / ↓ 大きくなる
#define EMOTION_AROUSAL_NORMALIZATION_DIVISOR 4.0     // ↑ arousal上がりにくい / ↓ 上がりやすい

// 表示しきい値
#define EMOTION_DOMINANT_WEAK_THRESHOLD 0.3           // ↑ 「弱い」判定が増える
#define EMOTION_DOMINANT_STRONG_THRESHOLD 0.6         // ↓ 「強い」判定が増える
#define EMOTION_VALENCE_POSITIVE_THRESHOLD 0.3        // ↓ 「ポジティブ」判定が増える
#define EMOTION_VALENCE_NEGATIVE_THRESHOLD -0.3       // ↑（0に近づける）「ネガティブ」判定が増える

// 文脈重み
#define EMOTION_CONTEXT_CONSTRUCTIVE_MULTIPLIER 0.8   // ↓ 建設的批判のダメージをさらに緩和
#define EMOTION_CONTEXT_HARSH_MULTIPLIER 1.25         // ↑ 強い批判語の影響を増幅
#define EMOTION_CONTEXT_STRONG_PRAISE_MULTIPLIER 1.1  // ↑ 強い賞賛語の影響を増幅

// 連鎖効果
#define EMOTION_CHAIN_ANGER_TO_DISGUST 0.35           // ↑ 怒りから嫌悪への波及が強くなる
#define EMOTION_CHAIN_SADNESS_TO_ANGER 0.15           // ↑ 悲しみ由来の怒りが出やすくなる
#define EMOTION_CHAIN_SURPRISE_TO_ANTICIPATION 0.2    // ↑ 驚きから期待へ転化しやすい
#define EMOTION_CHAIN_SURPRISE_TO_FEAR 0.1            // ↑ 驚きから恐れへ転化しやすい
#define EMOTION_CHAIN_JOY_TO_TRUST 0.2                // ↑ 喜びが信頼へ波及しやすい

// 感情別減衰倍率
#define EMOTION_DECAY_NEGATIVE_MULTIPLIER 0.7         // ↓ ネガ感情が長く残る / ↑ 早く鎮まる
#define EMOTION_DECAY_SURPRISE_MULTIPLIER 1.8         // ↑ 驚きが早く消える
#define EMOTION_DECAY_FEAR_ANTICIPATION_MULTIPLIER 1.2 // ↑ 恐れ/期待が早く消える
#define EMOTION_DECAY_POSITIVE_MULTIPLIER 0.9         // ↓ 喜び/信頼が長く残る
#define EMOTION_DECAY_RATE_MAX 0.99                   // 上限安全弁。大きすぎると1stepでほぼ消失
