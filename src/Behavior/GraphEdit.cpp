#include "GraphEdit.h"

#include <algorithm>

namespace KR::Behavior
{
    bool CanAddLink(const Graph& graph, NodeId from, NodeId to)
    {
        const Node* fromNode = graph.FindNode(from);
        const Node* toNode = graph.FindNode(to);
        if (fromNode == nullptr || toNode == nullptr || fromNode->kind == toNode->kind)
        {
            return false; // 状態と条件を、交互につなぐ(同じ種類どうしは、つなげない)
        }

        for (const Link& link : graph.links)
        {
            if (link.from == from && link.to == to)
            {
                return false; // 同じ線
            }
            // 条件の出力は 1 本・入力は 1 本まで
            if (fromNode->kind == NodeKind::Condition && link.from == from)
            {
                return false;
            }
            if (toNode->kind == NodeKind::Condition && link.to == to)
            {
                return false;
            }
        }
        return true;
    }

    bool AddLink(Graph& graph, NodeId from, NodeId to)
    {
        if (!CanAddLink(graph, from, to))
        {
            return false;
        }
        int priority = 0;
        for (const Link& link : graph.links)
        {
            if (link.from == from)
            {
                priority = std::max(priority, link.priority + 1);
            }
        }
        graph.links.push_back({ from, to, priority });
        return true;
    }

    bool RemoveLink(Graph& graph, NodeId from, NodeId to)
    {
        const auto removed = std::remove_if(graph.links.begin(), graph.links.end(),
            [&](const Link& link) { return link.from == from && link.to == to; });
        if (removed == graph.links.end())
        {
            return false;
        }
        graph.links.erase(removed, graph.links.end());
        return true;
    }
}
