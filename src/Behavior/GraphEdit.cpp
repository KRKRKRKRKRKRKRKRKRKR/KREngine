#include "GraphEdit.h"

#include "KR/Behavior/NodeTypes.h"

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

namespace KR::Behavior
{
    NodeId AddNode(Graph& graph, const std::string& type, float x, float y)
    {
        const NodeTypeInfo* info = FindNodeType(type);
        if (info == nullptr)
        {
            return 0;
        }

        // 手で作ったグラフなどで、nextNodeId が、今ある id より小さくても、重ならないようにする
        for (const Node& existing : graph.nodes)
        {
            graph.nextNodeId = std::max(graph.nextNodeId, existing.id + 1);
        }

        Node node;
        node.id = graph.nextNodeId++;
        node.kind = info->kind;
        node.type = type;
        node.posX = x;
        node.posY = y;
        for (const ParamSpec& spec : info->params)
        {
            node.params[spec.name] = spec.defaultValue;
        }
        graph.nodes.push_back(node);

        if (node.kind == NodeKind::State && graph.FindNode(graph.startNode) == nullptr)
        {
            graph.startNode = node.id;
        }
        return node.id;
    }

    bool RemoveNode(Graph& graph, NodeId id)
    {
        const auto removedNode = std::remove_if(graph.nodes.begin(), graph.nodes.end(),
            [id](const Node& node) { return node.id == id; });
        if (removedNode == graph.nodes.end())
        {
            return false;
        }
        graph.nodes.erase(removedNode, graph.nodes.end());

        std::erase_if(graph.links, [id](const Link& link) { return link.from == id || link.to == id; });
        if (graph.startNode == id)
        {
            graph.startNode = 0;
        }
        return true;
    }
}
