#pragma once

#include "KR/Behavior/Graph.h"

#include <string>
#include <vector>

namespace KR::Behavior
{
    // ノードの値 1 つの説明。エディタは、これから値の欄を自動で作る
    // 値の型は defaultValue の型で決まる(float なら小数の欄・bool ならチェック、など)
    struct ParamSpec
    {
        std::string name;        // JSON のキー("speed" など)
        std::string displayName; // 画面に出す日本語の名前
        ParamValue defaultValue;
        float minValue = 0.0f;   // float・int のときの範囲(エディタの欄の範囲)
        float maxValue = 0.0f;   // minValue == maxValue なら、範囲なし
        std::string visibleWhen; // 空でなければ、この名前の bool の値が true のときだけ、エディタに出す(例 keyY は includeVertical が true のときだけ)
    };

    // ノードの種類 1 つの説明
    struct NodeTypeInfo
    {
        NodeKind kind = NodeKind::State;
        std::string type;        // "Wait"・"PlayerWithin" など。登録の中で重ならない
        std::string displayName; // 画面に出す日本語の名前
        std::vector<ParamSpec> params;

        // 論理の条件(And・Or・Not)だけ:入力の数の範囲(Node::inputs の数)。葉の条件・状態は、0(入力なし)
        // minInputs < maxInputs なら、エディタで、入力を増やせる(And・Or)。同じなら、固定(Not)
        int minInputs = 0;
        int maxInputs = 0;

        // 論理の条件か(入力の条件を持つ = かつ・または・でない)
        bool IsLogic() const { return kind == NodeKind::Condition && maxInputs > 0; }
    };

    // 種類を登録する。同じ type が既にあるか、type が空なら、登録せず false を返す
    bool RegisterNodeType(NodeTypeInfo info);

    // type の説明を返す。無ければ nullptr(登録を足すと無効になるので、持ち続けない)
    const NodeTypeInfo* FindNodeType(const std::string& type);

    // 登録した順の一覧(エディタの追加メニュー用)
    const std::vector<NodeTypeInfo>& GetNodeTypes();
}
