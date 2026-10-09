#pragma once

// グラフを編集する操作(エンジンの中だけ。エディタ GraphEditor が使う)
// ルールを、画面から切り離して、確認ログで試せるようにしてある

#include "KR/Behavior/Graph.h"

namespace KR::Behavior
{
    // from の出力 → to の入力の線を、足してよいか
    // 許可:状態 → 条件・条件 → 状態。条件は、入力 1 本・出力 1 本まで。同じ線の重複は、不可。存在しない id は、不可
    // 状態は、出力も入力も、何本でもよい(出力が複数 = 分岐)
    bool CanAddLink(const Graph& graph, NodeId from, NodeId to);

    // 線を足す(CanAddLink が true のときだけ)。priority は、from から出る線の最後の次の番号。足したら true
    bool AddLink(Graph& graph, NodeId from, NodeId to);

    // 線を消す。消したら true
    bool RemoveLink(Graph& graph, NodeId from, NodeId to);
}
