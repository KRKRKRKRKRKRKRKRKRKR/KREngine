#pragma once

// グラフを編集する操作(エンジンの中だけ。エディタ GraphEditor が使う)
// ルールを、画面から切り離して、確認ログで試せるようにしてある
//
// 2 種類のつながり(UE のブループリントの、実行の線と、データの線のように):
//   流れ = 状態の「遷移の行」→ 次の状態(Transition::to)。行ごとに 1 つ
//   真偽 = 条件のノード → 遷移の行の入口(Transition::condition)/ 論理の条件の入力(Node::inputs)。入口は 1 つ(1 本まで)。条件の出口は、いくつでも
// つなげるときの規則:
//   ・遷移の行の condition には「条件」のノード、to には「状態」のノードだけ
//   ・論理の条件の入力にも、「条件」のノードだけ。自分自身は、不可
//   ・**循環を作る線は、拒否**(かつ・または・でない の入力をたどって、自分に戻る形)
//   ・0 を渡すと、つながりを外す

#include "KR/Behavior/Graph.h"

#include <string>

namespace KR::Behavior
{
    // ---- ノード ----

    // 登録された種類(type)のノードを、新しい id で足す。値は、登録の既定値。位置は (x, y)
    // 論理の条件は、入力のスロットを、最小の数だけ作る(つながっていない = 0)
    // id は graph.nextNodeId から発行して、増やす(消した id も、今ある id も、使い回さない)
    // 状態を足して、開始ノードがまだ無ければ、それを開始にする。足したノードの id を返す(type が未登録なら 0)
    NodeId AddNode(Graph& graph, const std::string& type, float x, float y);

    // ノードを消す。**他のノードの、そのノードへのつながり(遷移の行の条件・次の状態・論理の入力)は、外す**(行・スロットは残る)
    // 開始ノードを消したら、開始は「未指定(0)」になる。消したら true(id が無ければ false)
    bool RemoveNode(Graph& graph, NodeId id);

    // 開始ノードにする(状態だけ。条件・存在しない id は、false)
    bool SetStartNode(Graph& graph, NodeId id);

    // ---- 遷移の行(状態だけ)----

    // 状態の最後に、空の行(条件も次の状態も、つながっていない)を足す。足した行の番号を返す(状態でない・無い id は -1)
    int AddTransition(Graph& graph, NodeId state);

    // 行を消す(後ろの行は、1 つ詰まる)。消したら true
    bool RemoveTransition(Graph& graph, NodeId state, int index);

    // 行を、1 つ上(direction = -1)か 1 つ下(+1)に動かす(= 調べる順を変える)。先頭を上へ・最後を下へは、false
    bool MoveTransition(Graph& graph, NodeId state, int index, int direction);

    // 行の入口に、条件のノードをつなぐ(0 = 外す)。条件でないノード・無い id は、false
    bool SetTransitionCondition(Graph& graph, NodeId state, int index, NodeId condition);

    // 行の出口に、次の状態をつなぐ(0 = 外す)。状態でないノード・無い id は、false
    bool SetTransitionTarget(Graph& graph, NodeId state, int index, NodeId to);

    // ---- 論理の条件の入力(かつ・または・でない)----

    // 入力を 1 つ増やす(空のスロット)。登録の最大の数まで。増やせたら true
    bool AddInput(Graph& graph, NodeId logic, int* newIndex = nullptr);

    // 入力のスロットを、1 つ消す(後ろは詰まる)。登録の最小の数までしか、減らせない。消せたら true
    bool RemoveInput(Graph& graph, NodeId logic, int index);

    // 入力に、条件のノードをつなぐ(0 = 外す)。循環を作る・自分自身・条件でないノードは、false
    bool SetInput(Graph& graph, NodeId logic, int index, NodeId source);

    // from の条件が、(入力をたどって)target の条件に、依存しているか(自分自身も含む)。循環の判定に使う
    bool DependsOn(const Graph& graph, NodeId from, NodeId target);
}
