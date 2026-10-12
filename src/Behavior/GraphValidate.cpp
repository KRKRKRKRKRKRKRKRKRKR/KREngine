#include "KR/Behavior/GraphValidate.h"

#include "KR/Behavior/NodeTypes.h"

#include <set>

namespace KR::Behavior
{
    namespace
    {
        // 「待機」(#1) の形で、ノードを示す
        std::string Describe(const Node& node)
        {
            const NodeTypeInfo* info = FindNodeType(node.type);
            return "「" + (info ? info->displayName : node.type) + "」(#" + std::to_string(node.id) + ")";
        }
    }

    std::vector<GraphWarning> ValidateGraph(const Graph& graph)
    {
        std::vector<GraphWarning> warnings;

        const Node* start = graph.FindNode(graph.startNode);
        const bool hasStart = start != nullptr && start->kind == NodeKind::State;
        if (!hasStart)
        {
            warnings.push_back({ GraphWarningKind::NoStart, 0, "開始ノードがありません(状態のノードを右クリックして「ここから始める」)。今は、最初の状態から始まります" });
        }

        // 開始からたどり着ける所(線の向きどおりに、たどる)
        std::set<NodeId> reachable;
        if (hasStart)
        {
            std::vector<NodeId> pending{ start->id };
            while (!pending.empty())
            {
                const NodeId id = pending.back();
                pending.pop_back();
                if (!reachable.insert(id).second)
                {
                    continue;
                }
                for (const Link& link : graph.links)
                {
                    if (link.from == id)
                    {
                        pending.push_back(link.to);
                    }
                }
            }
        }

        for (const Node& node : graph.nodes)
        {
            bool hasIncoming = false;
            bool hasOutgoing = false;
            for (const Link& link : graph.links)
            {
                hasIncoming |= link.to == node.id;
                hasOutgoing |= link.from == node.id;
            }

            if (node.kind == NodeKind::State)
            {
                if (!hasOutgoing)
                {
                    warnings.push_back({ GraphWarningKind::StateDeadEnd, node.id,
                        Describe(node) + " から出る線がありません(この状態のまま、ずっと続きます)" });
                }
                if (hasStart && !reachable.contains(node.id))
                {
                    warnings.push_back({ GraphWarningKind::StateUnreachable, node.id,
                        Describe(node) + " は、開始ノードからたどり着けません(使われません)" });
                }
            }
            else
            {
                if (!hasIncoming)
                {
                    warnings.push_back({ GraphWarningKind::ConditionNoInput, node.id,
                        Describe(node) + " の入力がつながっていません(調べられることがありません)" });
                }
                if (!hasOutgoing)
                {
                    warnings.push_back({ GraphWarningKind::ConditionNoOutput, node.id,
                        Describe(node) + " の出力がつながっていません(成り立っても、移る先がありません)" });
                }
            }
        }
        return warnings;
    }
}
