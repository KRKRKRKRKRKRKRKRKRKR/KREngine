#pragma once

#include "KR/Behavior/Graph.h"

#include <string>

namespace KR::Behavior
{
    // 今の状態を、ゲームが読むための窓口(読むだけ)
    // Runner の中のグラフを指している。Runner を変更(Update・SetGraph など)すると、取り直す
    class StateView
    {
    public:
        StateView() = default;
        StateView(const Node* node, float timeInState, bool enteredThisFrame);

        NodeId GetNodeId() const { return node_ ? node_->id : 0; }
        // 状態の種類("Wait"・"Patrol"・"MoveToPlayer"・"Attack" など)
        const std::string& GetType() const;
        // この状態に入ってからの秒
        float GetTimeInState() const { return timeInState_; }
        // この状態に入った Update の直後か(入った最初のフレームだけ true。攻撃の開始の合図などに使う)
        bool EnteredThisFrame() const { return enteredThisFrame_; }

        // 状態の値(speed など)。無い・型が違うときは defaultValue
        float GetFloat(const std::string& name, float defaultValue = 0.0f) const;
        int GetInt(const std::string& name, int defaultValue = 0) const;
        bool GetBool(const std::string& name, bool defaultValue = false) const;
        std::string GetString(const std::string& name, const std::string& defaultValue = std::string()) const;

    private:
        const Node* node_ = nullptr;
        float timeInState_ = 0.0f;
        bool enteredThisFrame_ = false;
    };

    // 敵 1 体ぶんの「今どの状態か」。グラフ(GraphPtr)は、複数の敵で共有してよい
    class Runner
    {
    public:
        // 開始ノードの状態に入った状態で作る。開始ノードが正しくなければ、最初の状態から。
        // 状態が 1 つも無い(または null の)グラフなら、待機 1 つの既定のグラフを使う
        explicit Runner(GraphPtr graph);

        StateView GetCurrentState() const;
        const GraphPtr& GetGraph() const { return graph_; }

    private:
        GraphPtr graph_;
        NodeId currentNode_ = 0;
        float timeInState_ = 0.0f;
        bool enteredThisFrame_ = true;
    };
}
