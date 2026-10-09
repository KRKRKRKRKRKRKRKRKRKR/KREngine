#include "KR/Behavior/Runner.h"

#include <algorithm>

namespace KR::Behavior
{
    namespace
    {
        // 値 1 つを、型を指定して取り出す。無い・型が違うときは nullptr
        template <typename T>
        const T* FindParam(const Node* node, const std::string& name)
        {
            if (node == nullptr)
            {
                return nullptr;
            }
            const auto found = node->params.find(name);
            return (found != node->params.end()) ? std::get_if<T>(&found->second) : nullptr;
        }

        // 開始する状態の id。開始ノードが状態ならそれ、違えば最初の状態、無ければ 0
        NodeId ResolveStart(const Graph& graph)
        {
            const Node* start = graph.FindNode(graph.startNode);
            if (start != nullptr && start->kind == NodeKind::State)
            {
                return start->id;
            }
            const auto first = std::find_if(graph.nodes.begin(), graph.nodes.end(),
                [](const Node& node) { return node.kind == NodeKind::State; });
            return (first != graph.nodes.end()) ? first->id : 0;
        }
    }

    StateView::StateView(const Node* node, float timeInState, bool enteredThisFrame)
        : node_(node), timeInState_(timeInState), enteredThisFrame_(enteredThisFrame)
    {
    }

    const std::string& StateView::GetType() const
    {
        static const std::string empty;
        return node_ ? node_->type : empty;
    }

    float StateView::GetFloat(const std::string& name, float defaultValue) const
    {
        const float* value = FindParam<float>(node_, name);
        return value ? *value : defaultValue;
    }

    int StateView::GetInt(const std::string& name, int defaultValue) const
    {
        const int* value = FindParam<int>(node_, name);
        return value ? *value : defaultValue;
    }

    bool StateView::GetBool(const std::string& name, bool defaultValue) const
    {
        const bool* value = FindParam<bool>(node_, name);
        return value ? *value : defaultValue;
    }

    std::string StateView::GetString(const std::string& name, const std::string& defaultValue) const
    {
        const std::string* value = FindParam<std::string>(node_, name);
        return value ? *value : defaultValue;
    }

    Runner::Runner(GraphPtr graph)
        : graph_(std::move(graph))
    {
        if (graph_ != nullptr)
        {
            currentNode_ = ResolveStart(*graph_);
        }
        if (currentNode_ == 0)
        {
            graph_ = MakeDefaultGraph();
            currentNode_ = graph_->startNode;
        }
    }

    StateView Runner::GetCurrentState() const
    {
        return StateView(graph_->FindNode(currentNode_), timeInState_, enteredThisFrame_);
    }
}
