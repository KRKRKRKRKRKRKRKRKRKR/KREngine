#pragma once

// グラフを編集する操作(エンジンの中だけ。エディタ GraphEditor が使う)
// ルールを、画面から切り離して、確認ログで試せるようにしてある

#include "KR/Behavior/Graph.h"

#include <string>

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

namespace KR::Behavior
{
    // 登録された種類(type)のノードを、新しい id で足す。値は、登録の既定値。位置は (x, y)
    // id は graph.nextNodeId から発行して、増やす(消した id も、今ある id も、使い回さない)
    // 状態を足して、開始ノードがまだ無ければ、それを開始にする。足したノードの id を返す(type が未登録なら 0)
    NodeId AddNode(Graph& graph, const std::string& type, float x, float y);

    // ノードを消す。そのノードにつながる線も、消す。開始ノードを消したら、開始は「未指定(0)」になる
    // 消したら true(id が無ければ false)
    bool RemoveNode(Graph& graph, NodeId id);
}

namespace KR::Behavior
{
    // 開始ノードにする(状態だけ。条件・存在しない id は、false)
    bool SetStartNode(Graph& graph, NodeId id);

    // from から出る線のうち、to への線の順番(priority)を、1 つ上(direction = -1)か、1 つ下(+1)に動かす
    // 先頭を上へ・最後を下へは、何も起きない(false)。同じ priority の線が複数あっても、今の並び(OutgoingLinks の順)のまま、
    // 0, 1, 2 … と、振り直してから入れ替える(= 入れ替えたあとは、必ず 0 から連番)
    bool MoveLinkPriority(Graph& graph, NodeId from, NodeId to, int direction);
}
