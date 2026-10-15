#include "GraphEdit.h"

#include "KR/Behavior/NodeTypes.h"

#include <algorithm>
#include <set>
#include <vector>

namespace KR::Behavior
{
    namespace
    {
        Node* FindState(Graph& graph, NodeId id)
        {
            Node* node = graph.FindNode(id);
            return (node != nullptr && node->kind == NodeKind::State) ? node : nullptr;
        }

        // 論理の条件(かつ・または・でない)か。info は、登録の説明(見つかれば)
        Node* FindLogic(Graph& graph, NodeId id, const NodeTypeInfo** info = nullptr)
        {
            Node* node = graph.FindNode(id);
            if (node == nullptr || node->kind != NodeKind::Condition)
            {
                return nullptr;
            }
            const NodeTypeInfo* type = FindNodeType(node->type);
            if (type == nullptr || !type->IsLogic())
            {
                return nullptr;
            }
            if (info != nullptr)
            {
                *info = type;
            }
            return node;
        }

        bool IsValidIndex(size_t size, int index)
        {
            return index >= 0 && static_cast<size_t>(index) < size;
        }
    }

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
        node.inputs.assign(static_cast<size_t>(info->minInputs), 0); // 論理の条件:入力のスロット(つながっていない)
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

        // 他のノードの、そのノードへのつながりを外す(行・スロットは、残す = つなぎ直せる)
        for (Node& node : graph.nodes)
        {
            for (Transition& transition : node.transitions)
            {
                if (transition.condition == id)
                {
                    transition.condition = 0;
                }
                if (transition.to == id)
                {
                    transition.to = 0;
                }
            }
            for (NodeId& input : node.inputs)
            {
                if (input == id)
                {
                    input = 0;
                }
            }
        }
        if (graph.startNode == id)
        {
            graph.startNode = 0;
        }
        return true;
    }

    bool SetStartNode(Graph& graph, NodeId id)
    {
        if (FindState(graph, id) == nullptr)
        {
            return false;
        }
        graph.startNode = id;
        return true;
    }

    int AddTransition(Graph& graph, NodeId state)
    {
        Node* node = FindState(graph, state);
        if (node == nullptr)
        {
            return -1;
        }
        node->transitions.emplace_back();
        return static_cast<int>(node->transitions.size()) - 1;
    }

    bool RemoveTransition(Graph& graph, NodeId state, int index)
    {
        Node* node = FindState(graph, state);
        if (node == nullptr || !IsValidIndex(node->transitions.size(), index))
        {
            return false;
        }
        node->transitions.erase(node->transitions.begin() + index);
        return true;
    }

    bool MoveTransition(Graph& graph, NodeId state, int index, int direction)
    {
        Node* node = FindState(graph, state);
        if (node == nullptr || !IsValidIndex(node->transitions.size(), index) || (direction != -1 && direction != 1))
        {
            return false;
        }
        const int target = index + direction;
        if (!IsValidIndex(node->transitions.size(), target))
        {
            return false;
        }
        std::swap(node->transitions[static_cast<size_t>(index)], node->transitions[static_cast<size_t>(target)]);
        return true;
    }

    bool SetTransitionCondition(Graph& graph, NodeId state, int index, NodeId condition)
    {
        Node* node = FindState(graph, state);
        if (node == nullptr || !IsValidIndex(node->transitions.size(), index))
        {
            return false;
        }
        if (condition != 0)
        {
            const Node* source = graph.FindNode(condition);
            if (source == nullptr || source->kind != NodeKind::Condition)
            {
                return false; // 条件のノードだけ
            }
        }
        node->transitions[static_cast<size_t>(index)].condition = condition;
        return true;
    }

    bool SetTransitionTarget(Graph& graph, NodeId state, int index, NodeId to)
    {
        Node* node = FindState(graph, state);
        if (node == nullptr || !IsValidIndex(node->transitions.size(), index))
        {
            return false;
        }
        if (to != 0 && FindState(graph, to) == nullptr)
        {
            return false; // 状態のノードだけ
        }
        node->transitions[static_cast<size_t>(index)].to = to;
        return true;
    }

    bool AddInput(Graph& graph, NodeId logic, int* newIndex)
    {
        const NodeTypeInfo* info = nullptr;
        Node* node = FindLogic(graph, logic, &info);
        if (node == nullptr || static_cast<int>(node->inputs.size()) >= info->maxInputs)
        {
            return false;
        }
        node->inputs.push_back(0);
        if (newIndex != nullptr)
        {
            *newIndex = static_cast<int>(node->inputs.size()) - 1;
        }
        return true;
    }

    bool RemoveInput(Graph& graph, NodeId logic, int index)
    {
        const NodeTypeInfo* info = nullptr;
        Node* node = FindLogic(graph, logic, &info);
        if (node == nullptr || !IsValidIndex(node->inputs.size(), index) || static_cast<int>(node->inputs.size()) <= info->minInputs)
        {
            return false;
        }
        node->inputs.erase(node->inputs.begin() + index);
        return true;
    }

    bool DependsOn(const Graph& graph, NodeId from, NodeId target)
    {
        std::set<NodeId> visited;
        std::vector<NodeId> pending{ from };
        while (!pending.empty())
        {
            const NodeId id = pending.back();
            pending.pop_back();
            if (id == 0 || !visited.insert(id).second)
            {
                continue;
            }
            if (id == target)
            {
                return true;
            }
            if (const Node* node = graph.FindNode(id))
            {
                for (const NodeId input : node->inputs)
                {
                    pending.push_back(input);
                }
            }
        }
        return false;
    }

    bool SetInput(Graph& graph, NodeId logic, int index, NodeId source)
    {
        Node* node = FindLogic(graph, logic);
        if (node == nullptr || !IsValidIndex(node->inputs.size(), index))
        {
            return false;
        }
        if (source != 0)
        {
            const Node* sourceNode = graph.FindNode(source);
            if (sourceNode == nullptr || sourceNode->kind != NodeKind::Condition)
            {
                return false; // 条件のノードだけ
            }
            if (DependsOn(graph, source, logic))
            {
                return false; // source が(入力をたどって)logic に依存している = つなぐと、循環になる(自分自身も含む)
            }
        }
        node->inputs[static_cast<size_t>(index)] = source;
        return true;
    }
}

namespace KR::Behavior
{
    namespace
    {
        // 出力のピンと、入力のピンを、(出力, 入力)の順に並べる。同じ向きどうしなら、false
        bool OrderPins(PinRef a, PinRef b, PinRef& output, PinRef& input)
        {
            if (a.IsOutput() == b.IsOutput())
            {
                return false;
            }
            output = a.IsOutput() ? a : b;
            input = a.IsOutput() ? b : a;
            return true;
        }
    }

    bool CanConnectPins(const Graph& graph, PinRef a, PinRef b)
    {
        PinRef output;
        PinRef input;
        if (!a.IsValid() || !b.IsValid() || !OrderPins(a, b, output, input))
        {
            return false;
        }
        const Node* outputNode = graph.FindNode(output.node);
        const Node* inputNode = graph.FindNode(input.node);
        if (outputNode == nullptr || inputNode == nullptr)
        {
            return false;
        }

        if (output.type == PinType::RowFlowOut && input.type == PinType::FlowIn)
        {
            // 遷移の行の出口 → 状態の「入」
            return outputNode->kind == NodeKind::State && inputNode->kind == NodeKind::State
                && output.index >= 0 && static_cast<size_t>(output.index) < outputNode->transitions.size();
        }
        if (output.type == PinType::ConditionOut && input.type == PinType::RowCondition)
        {
            // 条件の出口 → 遷移の行の条件の入口
            return outputNode->kind == NodeKind::Condition && inputNode->kind == NodeKind::State
                && input.index >= 0 && static_cast<size_t>(input.index) < inputNode->transitions.size();
        }
        if (output.type == PinType::ConditionOut && input.type == PinType::LogicInput)
        {
            // 条件の出口 → 論理の入力(自分自身・循環は、不可)
            return outputNode->kind == NodeKind::Condition && inputNode->kind == NodeKind::Condition
                && input.index >= 0 && static_cast<size_t>(input.index) < inputNode->inputs.size()
                && !DependsOn(graph, output.node, input.node);
        }
        return false; // 流れと真偽の混ぜ・同じ種類の出口・入口 など
    }

    bool ConnectPins(Graph& graph, PinRef a, PinRef b)
    {
        if (!CanConnectPins(graph, a, b))
        {
            return false;
        }
        PinRef output;
        PinRef input;
        OrderPins(a, b, output, input);
        switch (input.type)
        {
        case PinType::FlowIn: return SetTransitionTarget(graph, output.node, output.index, input.node);
        case PinType::RowCondition: return SetTransitionCondition(graph, input.node, input.index, output.node);
        case PinType::LogicInput: return SetInput(graph, input.node, input.index, output.node);
        default: return false;
        }
    }

    bool DisconnectPin(Graph& graph, PinRef pin)
    {
        switch (pin.type)
        {
        case PinType::RowFlowOut: return SetTransitionTarget(graph, pin.node, pin.index, 0);
        case PinType::RowCondition: return SetTransitionCondition(graph, pin.node, pin.index, 0);
        case PinType::LogicInput: return SetInput(graph, pin.node, pin.index, 0);
        default: return false;
        }
    }
}
