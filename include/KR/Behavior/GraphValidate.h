#pragma once

#include "KR/Behavior/Graph.h"

#include <string>
#include <vector>

namespace KR::Behavior
{
    enum class GraphWarningKind
    {
        NoStart,              // 開始ノードが、無い(または、状態でない)
        StateDeadEnd,         // 使える遷移の行が 1 つも無い状態(その状態のまま、ずっと続く)
        TransitionIncomplete, // 遷移の行で、条件の入口・次の状態の出口が、つながっていない(その行は、使われない)
        LogicInputMissing,    // かつ・または・でない の入力が、つながっていない(かつ・でない は、成り立たない)
        ConditionUnused,      // どの遷移の行・論理の入力にも、使われていない条件(調べられることがない)
        ConditionCycle,       // 論理の条件が、循環している(ツールでは作れない。ファイルを、手で直したときなど)
        StateUnreachable,     // 開始から、たどり着けない状態
    };

    struct GraphWarning
    {
        GraphWarningKind kind;
        NodeId node = 0;     // 対象のノード(NoStart は 0)
        int index = -1;      // 対象の行(遷移の行)・入力の番号。無ければ -1
        std::string message; // 日本語の説明(画面に出す)
    };

    // グラフの、つなぎ忘れなどを調べる。警告があっても、グラフは動く(Runner は、安全に動く)。エディタが、知らせるための物
    // 並びは、ノードの並びの順(同じノードの警告は、続けて並ぶ)。警告が無ければ、空
    std::vector<GraphWarning> ValidateGraph(const Graph& graph);
}
