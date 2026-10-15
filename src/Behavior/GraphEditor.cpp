// GraphEditor(ノードエディタの部品)
// **作り直し中(C5)**:データの形が変わった(遷移の行・論理の条件 = docs/Behavior.md の 15 節)ので、imnodes で作った、前の描き方・操作は、いったん外した。
// imgui-node-editor(ズームあり)で、C5a〜C5c に、書き直す。それまでは、ツールの画面は、警告の一覧と、この説明だけ(グラフは、描かない・編集できない)。
// 警告の一覧(DrawWarnings)は、新しい形のまま、動く。

#include "KR/Behavior/GraphEditor.h"

#include "KR/Behavior/GraphValidate.h"

#include <imgui.h>

#include <vector>

namespace KR::Behavior
{
    struct GraphEditor::Impl
    {
        NodeId observedNode = 0;     // 光らせるノード(ゲームの敵の今の状態。0 = 光らせない。C5a で、描く)
        bool observedPaused = false; // ゲームが止まっている(灰色で光らせる)
    };

    GraphEditor::GraphEditor()
        : impl_(std::make_unique<Impl>())
    {
    }

    GraphEditor::~GraphEditor() = default;

    void GraphEditor::Reset()
    {
    }

    bool GraphEditor::Draw(Graph& graph)
    {
        ImGui::TextWrapped("(ノードエディタは、作り直し中です。imgui-node-editor で、拡大縮小・遷移の行ごとのピン・かつ / または / でない のノードに、対応させます。"
            "今は、グラフを描けません・編集できません。ノード %d 個)", static_cast<int>(graph.nodes.size()));
        return false;
    }

    void GraphEditor::DrawWarnings(const Graph& graph)
    {
        const std::vector<GraphWarning> warnings = ValidateGraph(graph);
        if (warnings.empty())
        {
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.55f, 1.0f), "警告なし");
            return;
        }
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "警告 %d 件", static_cast<int>(warnings.size()));
        for (const GraphWarning& warning : warnings)
        {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", warning.message.c_str());
        }
    }
}

namespace KR::Behavior
{
    // 他の担当が足した機能(ツールの、動いている敵の今の状態を光らせる = Observe)。C5a の書き直しで、描く側も、引き継ぐ
    void GraphEditor::SetObservedNode(NodeId node, bool paused)
    {
        impl_->observedNode = node;
        impl_->observedPaused = paused;
    }
}
