#pragma once

#include "KR/Behavior/Graph.h"

#include <string>
#include <vector>

namespace KR::Behavior
{
    enum class GraphWarningKind
    {
        NoStart,            // 開始ノードが、無い(または、状態でない)
        StateDeadEnd,       // 状態から出る線が無い(その状態のまま、ずっと続く)
        ConditionNoInput,   // 条件の入力が、つながっていない(調べられることがない)
        ConditionNoOutput,  // 条件の出力が、つながっていない(成り立っても、どこへも移れない)
        StateUnreachable,   // 開始から、たどり着けない状態
    };

    struct GraphWarning
    {
        GraphWarningKind kind;
        NodeId node = 0;     // 対象のノード(NoStart は 0)
        std::string message; // 日本語の説明(画面に出す)
    };

    // グラフの、つなぎ忘れなどを調べる。警告があっても、グラフは動く(Runner は、安全に動く)。エディタが、知らせるための物
    // 並びは、ノードの並びの順(同じノードの警告は、続けて並ぶ)。警告が無ければ、空
    std::vector<GraphWarning> ValidateGraph(const Graph& graph);
}
