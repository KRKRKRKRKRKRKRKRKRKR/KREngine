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

    // ノードの番号。グラフの中で重ならない(消した番号は、使い回さない)
    using NodeId = int;

    // ノードの大きな分類。状態 = 敵がしていること / 条件 = 状態と状態の間の「移る理由」
    enum class NodeKind
    {
        State,
        Condition,
    };

    // ノード 1 つ。type は、登録された種類の名前("Wait"・"PlayerWithin" など)
    struct Node
    {
        NodeId id = 0;
        NodeKind kind = NodeKind::State;
        std::string type;
        float posX = 0.0f; // エディタ上の位置(動きには関係しない)
        float posY = 0.0f;
        ParamMap params;
    };

    // 線 1 本:from → to。意味は「状態 → 条件 → 状態」
    // 同じ from から複数の線が出るとき、priority の小さい順に調べる
    struct Link
    {
        NodeId from = 0;
        NodeId to = 0;
        int priority = 0;
    };

    // 敵 1 種類の行動のグラフ(= 1 ファイル)。読み込んだあとは変えない(GraphPtr で共有する)
    struct Graph
    {
        int version = 1;
        std::string graphType = "fsm";
        NodeId startNode = 0; // 最初の状態。0 = 未指定
        std::vector<Node> nodes;
        std::vector<Link> links;

        // id のノードを返す。無ければ nullptr(返した値は、nodes を変えると無効になる)
        const Node* FindNode(NodeId id) const;
        Node* FindNode(NodeId id);

        // id のノードから出る線を、priority の小さい順(同じなら、links の並びの順)で返す
        std::vector<Link> OutgoingLinks(NodeId id) const;
    };

    using GraphPtr = std::shared_ptr<const Graph>;
}

namespace KR::Behavior
{
    // 安全な既定のグラフ:待機 1 つ(読めなかったファイル・状態が無いグラフの代わり)
    GraphPtr MakeDefaultGraph();
}
