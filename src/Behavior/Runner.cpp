#include "KR/Behavior/Runner.h"

#include "Condition.h"

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
    {
        SetGraphAndStart(std::move(graph));
    }

    void Runner::SetGraphAndStart(GraphPtr graph)
    {
        graph_ = std::move(graph);
        currentNode_ = (graph_ != nullptr) ? ResolveStart(*graph_) : 0;
        if (currentNode_ == 0)
        {
            graph_ = MakeDefaultGraph();
            currentNode_ = graph_->startNode;
        }
        timeInState_ = 0.0f;
        enteredThisFrame_ = true;
    }

    void Runner::SetGraph(GraphPtr graph)
    {
        // 今の状態の id が、新しいグラフにも状態として残っていれば、引き継ぐ(時間もそのまま)
        if (graph != nullptr)
        {
            const Node* same = graph->FindNode(currentNode_);
            if (same != nullptr && same->kind == NodeKind::State)
            {
                graph_ = std::move(graph);
                return;
            }
        }
        SetGraphAndStart(std::move(graph)); // 無ければ、開始の状態に戻る
    }

    void Runner::Reset()
    {
        SetGraphAndStart(graph_);
    }

    void Runner::Update(float deltaTime, const Context& context)
    {
        enteredThisFrame_ = false;
        timeInState_ += deltaTime;

        // 状態 → 条件 → 状態。状態から出る線(priority 順)のうち、最初に成り立つ条件の先へ移る
        for (const Link& toCondition : graph_->OutgoingLinks(currentNode_))
        {
            const Node* condition = graph_->FindNode(toCondition.to);
            if (condition == nullptr || condition->kind != NodeKind::Condition)
            {
                continue; // 状態へ直接つなぐ線は、使わない(条件のない移動は、作らない)
            }
            if (!EvaluateCondition(*condition, context, timeInState_))
            {
                continue;
            }

            // 条件の出力(1 本のはず。複数あれば、priority の小さい順で最初の状態)
            for (const Link& toState : graph_->OutgoingLinks(condition->id))
            {
                const Node* next = graph_->FindNode(toState.to);
                if (next != nullptr && next->kind == NodeKind::State)
                {
                    currentNode_ = next->id;
                    timeInState_ = 0.0f;
                    enteredThisFrame_ = true;
                    return; // 1 回の Update で移るのは、1 回まで
                }
            }
            // 出力がつながっていない条件は、成り立っても、移れない → 次の線を調べる
        }
    }

    StateView Runner::GetCurrentState() const
    {
        return StateView(graph_->FindNode(currentNode_), timeInState_, enteredThisFrame_);
    }
}
