#include "KR/Behavior/Graph.h"

#include "GraphEdit.h"

#include <utility>

namespace KR::Behavior
{
    const Node* Graph::FindNode(NodeId id) const
    {
        for (const Node& node : nodes)
        {
            if (node.id == id)
            {
                return &node;
            }
        }
        return nullptr;
    }

    Node* Graph::FindNode(NodeId id)
    {
        return const_cast<Node*>(std::as_const(*this).FindNode(id));
    }
}

namespace KR::Behavior
{
    GraphPtr MakeDefaultGraph()
    {
        Graph graph;
        Node wait;
        wait.id = 1;
        wait.kind = NodeKind::State;
        wait.type = "Wait";
        graph.startNode = wait.id;
        graph.nextNodeId = 2;
        graph.nodes.push_back(wait);
        return std::make_shared<const Graph>(std::move(graph));
    }
}

namespace KR::Behavior
{
    Graph MakeStarterGraph()
    {
        Graph graph;
        const NodeId wait = AddNode(graph, "Wait", 40.0f, 60.0f);
        const NodeId near = AddNode(graph, "PlayerWithin", 40.0f, 330.0f);
        const NodeId move = AddNode(graph, "MoveToPlayer", 560.0f, 60.0f);
        const NodeId close = AddNode(graph, "PlayerWithin", 560.0f, 330.0f);
        const NodeId attack = AddNode(graph, "Attack", 1080.0f, 60.0f);
        const NodeId finished = AddNode(graph, "AttackFinished", 1080.0f, 440.0f);
        if (Node* node = graph.FindNode(near))
        {
            node->params["radius"] = 3.0f;
        }
        if (Node* node = graph.FindNode(close))
        {
            node->params["radius"] = 1.0f;
        }
        // 待機 → [半径 3 以内] → 移動 → [半径 1 以内] → 攻撃 → [攻撃が終わった] → 待機
        const int a = AddTransition(graph, wait);
        SetTransitionCondition(graph, wait, a, near);
        SetTransitionTarget(graph, wait, a, move);
        const int b = AddTransition(graph, move);
        SetTransitionCondition(graph, move, b, close);
        SetTransitionTarget(graph, move, b, attack);
        const int c = AddTransition(graph, attack);
        SetTransitionCondition(graph, attack, c, finished);
        SetTransitionTarget(graph, attack, c, wait);
        graph.startNode = wait;
        return graph;
    }
}
