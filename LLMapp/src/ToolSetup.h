#pragma once

#include "EmotionalAgent.h"

/**
 * @brief 標準ツール群をエージェントへ登録する
 *
 * main関数の肥大化を防ぐため、ツールI/Oの登録処理を分離。
 * ツールの追加・変更はこの関数内を編集する。
 */
void register_default_tools(EmotionalAgent& agent);
