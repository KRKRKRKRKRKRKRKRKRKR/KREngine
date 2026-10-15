#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace KR::Behavior
{
    // ノードが持てる値 1 つ:小数(float)・整数(int)・真偽(bool)・文字(string)のどれか
    // 例:速さ = float・ダメージ = int・縦も含める = bool・状況のキー名 = string
    using ParamValue = std::variant<float, int, bool, std::string>;

    // ノードの値の一覧:名前 → 値。名前の順に並ぶ(JSON に書いたとき、順番が、いつも同じになる)
    using ParamMap = std::map<std::string, ParamValue>;

    // ノードの番号。グラフの中で重ならない(消した番号は、使い回さない)。0 = 「つながっていない」
    using NodeId = int;

    // ノードの大きな分類。状態 = 敵がしていること / 条件 = 「成り立つか」(真偽)を返す部品
    enum class NodeKind
    {
        State,
        Condition,
    };

    // 状態の「遷移の行」1 つ:「もし condition が成り立つなら、to の状態へ移る」(UE のブランチのように、行ごとに、別のピン)
    // 0 = つながっていない(condition が 0 の行は、成り立たない = 移らない)
    struct Transition
    {
        NodeId condition = 0; // 条件のノードの id(葉の条件でも、かつ・または・でない でもよい)
        NodeId to = 0;        // 次の状態のノードの id
    };

    // ノード 1 つ。type は、登録された種類の名前("Wait"・"PlayerWithin"・"And" など)
    struct Node
    {
        NodeId id = 0;
        NodeKind kind = NodeKind::State;
        std::string type;
        float posX = 0.0f; // エディタ上の位置(動きには関係しない)
        float posY = 0.0f;
        ParamMap params;

        // 状態だけ:遷移の行。**並びの順 = 調べる順**(上の行が先。最初に成り立った行の、次の状態へ移る)
        std::vector<Transition> transitions;

        // 論理の条件(And・Or・Not)だけ:入力の条件のノードの id。0 = つながっていない(入力の数は、登録の範囲の中で、増やせる)
        std::vector<NodeId> inputs;
    };

    // 敵 1 種類の行動のグラフ(= 1 ファイル。JSON 版 2)。読み込んだあとは変えない(GraphPtr で共有する)
    //   流れ = 状態の遷移の行 → 次の状態 / 真偽 = 条件のノード → 遷移の行(または、論理の条件の入力)
    struct Graph
    {
        int version = 2;
        std::string graphType = "fsm";
        NodeId startNode = 0; // 最初の状態。0 = 未指定
        NodeId nextNodeId = 1; // 次に発行するノードの id。ノードを消しても、その id は使い回さない(増える一方。JSON に保存する)
        std::vector<Node> nodes;

        // id のノードを返す。無ければ nullptr(返した値は、nodes を変えると無効になる)
        const Node* FindNode(NodeId id) const;
        Node* FindNode(NodeId id);
    };

    using GraphPtr = std::shared_ptr<const Graph>;
}

namespace KR::Behavior
{
    // 安全な既定のグラフ:待機 1 つ(読めなかったファイル・状態が無いグラフの代わり)
    GraphPtr MakeDefaultGraph();
}

namespace KR::Behavior
{
    // 見本のグラフ(依頼の例):待機 → [プレイヤーが半径 3 以内] → 移動 → [半径 1 以内] → 攻撃 → [攻撃が終わった] → 待機
    // エディタの、はじめの画面・使い方の見本・確認に使う。登録されたノードの種類の、既定の値で作る
    Graph MakeStarterGraph();
}
