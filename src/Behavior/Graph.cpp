#include "KR/Behavior/Graph.h"

#include "GraphEdit.h"

#include <algorithm>
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

    std::vector<Link> Graph::OutgoingLinks(NodeId id) const
    {
        std::vector<Link> result;
        for (const Link& link : links)
        {
            if (link.from == id)
            {
                result.push_back(link);
            }
        }
        // 同じ priority は、元の並びを保つ(stable)
        std::stable_sort(result.begin(), result.end(),
            [](const Link& a, const Link& b) { return a.priority < b.priority; });
        return result;
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
        const NodeId near = AddNode(graph, "PlayerWithin", 260.0f, 60.0f);
        const NodeId move = AddNode(graph, "MoveToPlayer", 510.0f, 60.0f);
        const NodeId close = AddNode(graph, "PlayerWithin", 730.0f, 60.0f);
        const NodeId attack = AddNode(graph, "Attack", 980.0f, 60.0f);
        const NodeId finished = AddNode(graph, "AttackFinished", 510.0f, 330.0f);
        if (Node* node = graph.FindNode(near))
        {
            node->params["radius"] = 3.0f;
        }
        if (Node* node = graph.FindNode(close))
        {
            node->params["radius"] = 1.0f;
        }
        AddLink(graph, wait, near);
        AddLink(graph, near, move);
        AddLink(graph, move, close);
        AddLink(graph, close, attack);
        AddLink(graph, attack, finished);
        AddLink(graph, finished, wait);
        graph.startNode = wait;
        return graph;
    }
}
