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

        // 状態の遷移の行を、上から(並びの順 = 調べる順)。最初に成り立った行の、次の状態へ移る
        //   行の入口(条件)が 0(つながっていない)・出口(次の状態)が 0 の行は、使わない(次の行を調べる)
        //   入口の条件は、葉でも、かつ・または・でない(ネスト)でもよい
        const Node* state = graph_->FindNode(currentNode_);
        if (state == nullptr)
        {
            return;
        }
        for (const Transition& row : state->transitions)
        {
            if (row.condition == 0 || row.to == 0)
            {
                continue;
            }
            const Node* next = graph_->FindNode(row.to);
            if (next == nullptr || next->kind != NodeKind::State)
            {
                continue;
            }
            if (EvaluateConditionTree(*graph_, row.condition, context, timeInState_))
            {
                currentNode_ = next->id;
                timeInState_ = 0.0f;
                enteredThisFrame_ = true;
                return; // 1 回の Update で移るのは、1 回まで
            }
        }
    }

    StateView Runner::GetCurrentState() const
    {
        return StateView(graph_->FindNode(currentNode_), timeInState_, enteredThisFrame_);
    }
}
