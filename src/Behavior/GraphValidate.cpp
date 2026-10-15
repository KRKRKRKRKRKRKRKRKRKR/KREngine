#include "KR/Behavior/GraphValidate.h"

#include "GraphEdit.h"
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

        // 使える行 = 入口も出口も、つながっている行(Runner が、実際に使う行)
        bool IsUsable(const Transition& row)
        {
            return row.condition != 0 && row.to != 0;
        }
    }

    std::vector<GraphWarning> ValidateGraph(const Graph& graph)
    {
        std::vector<GraphWarning> warnings;

        const Node* start = graph.FindNode(graph.startNode);
        const bool hasStart = start != nullptr && start->kind == NodeKind::State;
        if (!hasStart)
        {
            warnings.push_back({ GraphWarningKind::NoStart, 0, -1, "開始ノードがありません(状態のノードを右クリックして「ここから始める」)。今は、最初の状態から始まります" });
        }

        // 開始からたどり着ける状態(使える行だけを、たどる)
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
                if (const Node* node = graph.FindNode(id))
                {
                    for (const Transition& row : node->transitions)
                    {
                        if (IsUsable(row))
                        {
                            pending.push_back(row.to);
                        }
                    }
                }
            }
        }

        // 使われている条件(遷移の行の入口・論理の入力。使われている条件の入力も、たどる)
        std::set<NodeId> used;
        {
            std::vector<NodeId> pending;
            for (const Node& node : graph.nodes)
            {
                for (const Transition& row : node.transitions)
                {
                    if (row.condition != 0)
                    {
                        pending.push_back(row.condition);
                    }
                }
            }
            while (!pending.empty())
            {
                const NodeId id = pending.back();
                pending.pop_back();
                if (!used.insert(id).second)
                {
                    continue;
                }
                if (const Node* node = graph.FindNode(id))
                {
                    for (const NodeId input : node->inputs)
                    {
                        if (input != 0)
                        {
                            pending.push_back(input);
                        }
                    }
                }
            }
        }

        for (const Node& node : graph.nodes)
        {
            if (node.kind == NodeKind::State)
            {
                bool anyUsable = false;
                for (size_t i = 0; i < node.transitions.size(); ++i)
                {
                    const Transition& row = node.transitions[i];
                    anyUsable |= IsUsable(row);
                    if (!IsUsable(row))
                    {
                        const std::string number = std::to_string(i + 1);
                        const char* what = (row.condition == 0 && row.to == 0) ? "の条件も、次の状態も、つながっていません"
                            : (row.condition == 0) ? "の条件が、つながっていません"
                            : "の次の状態が、つながっていません";
                        warnings.push_back({ GraphWarningKind::TransitionIncomplete, node.id, static_cast<int>(i),
                            Describe(node) + " の遷移の行 " + number + " " + what + "(この行は、使われません)" });
                    }
                }
                if (!anyUsable)
                {
                    warnings.push_back({ GraphWarningKind::StateDeadEnd, node.id, -1,
                        Describe(node) + " から出る、使える遷移がありません(この状態のまま、ずっと続きます)" });
                }
                if (hasStart && !reachable.contains(node.id))
                {
                    warnings.push_back({ GraphWarningKind::StateUnreachable, node.id, -1,
                        Describe(node) + " は、開始ノードからたどり着けません(使われません)" });
                }
            }
            else
            {
                const NodeTypeInfo* info = FindNodeType(node.type);
                if (info != nullptr && info->IsLogic())
                {
                    for (size_t i = 0; i < node.inputs.size(); ++i)
                    {
                        if (node.inputs[i] == 0)
                        {
                            warnings.push_back({ GraphWarningKind::LogicInputMissing, node.id, static_cast<int>(i),
                                Describe(node) + " の入力 " + std::to_string(i + 1) + " が、つながっていません" });
                        }
                        else if (DependsOn(graph, node.inputs[i], node.id))
                        {
                            warnings.push_back({ GraphWarningKind::ConditionCycle, node.id, static_cast<int>(i),
                                Describe(node) + " の入力 " + std::to_string(i + 1) + " が、循環しています(評価すると、偽になります)" });
                        }
                    }
                }
                if (!used.contains(node.id))
                {
                    warnings.push_back({ GraphWarningKind::ConditionUnused, node.id, -1,
                        Describe(node) + " は、どの遷移の行にも、使われていません(調べられることがありません)" });
                }
            }
        }
        return warnings;
    }
}
