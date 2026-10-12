#pragma once

#include "KR/Behavior/Graph.h"
#include "KR/Behavior/Runner.h"

namespace KR::Behavior
{
    // デバッグ用:グラフを、ImGui の窓の中に、読むだけで描く(動かせない・編集できない)。動いている敵の「今の状態」のノードを、橙で光らせる
    // ゲームの OnImGui の中で、ImGui::Begin(...) と End の間に呼ぶ(例 DrawGraphDebug(runner))
    // 窓の残りの広さに、グラフ全体を収める(大きいグラフは、縮小して。拡大はしない)。imnodes は使わない(ImGui の描画だけ = 文脈の管理が要らない)
    //   光るノード = 今の状態 / 橙の線 = 今の状態から、出る線(次に移るかもしれない道)/ 左下 = 今の状態の名前と、入ってからの時間
    // width / height が 0 なら、窓の残りの広さ
    void DrawGraphDebug(const Graph& graph, const Runner& runner, float width = 0.0f, float height = 0.0f);

    // Runner が持っているグラフを、そのまま描く(いちばん簡単な呼び方)
    void DrawGraphDebug(const Runner& runner, float width = 0.0f, float height = 0.0f);
}
