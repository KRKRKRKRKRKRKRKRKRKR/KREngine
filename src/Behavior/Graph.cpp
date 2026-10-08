#include "KR/Behavior/Graph.h"

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
