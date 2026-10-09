#include "KR/Behavior/GraphEditor.h"

#include "KR/Behavior/NodeTypes.h"

#include <imgui.h>
#include <imnodes.h>

#include <cmath>
#include <set>
#include <string>

namespace KR::Behavior
{
    namespace
    {
        // imnodes の id は、ノード・ピン・線で、別々の番号の空間。ピンは、ノードの id から作る(入力 = 1・出力 = 2)
        int InputPinId(NodeId node) { return node * 4 + 1; }
        int OutputPinId(NodeId node) { return node * 4 + 2; }

        constexpr ImU32 StateColor = IM_COL32(50, 100, 190, 255);
        constexpr ImU32 StateColorHover = IM_COL32(70, 125, 220, 255);
        constexpr ImU32 ConditionColor = IM_COL32(40, 150, 90, 255);
        constexpr ImU32 ConditionColorHover = IM_COL32(60, 180, 110, 255);
        constexpr ImU32 StartOutlineColor = IM_COL32(255, 160, 40, 255);
    }

    struct GraphEditor::Impl
    {
        ImNodesContext* context = nullptr;
        std::set<NodeId> placed;       // 位置を、imnodes に渡した(= 以後は imnodes が持つ)ノード
        const Graph* lastGraph = nullptr;
    };

    GraphEditor::GraphEditor()
        : impl_(std::make_unique<Impl>())
    {
        impl_->context = ImNodes::CreateContext();
    }

    GraphEditor::~GraphEditor()
    {
        ImNodes::DestroyContext(impl_->context);
    }

    void GraphEditor::Reset()
    {
        impl_->placed.clear();
        impl_->lastGraph = nullptr;
    }

    bool GraphEditor::Draw(Graph& graph)
    {
        ImNodes::SetCurrentContext(impl_->context);
        if (impl_->lastGraph != &graph)
        {
            impl_->placed.clear(); // 別のグラフ
            impl_->lastGraph = &graph;
        }

        ImNodes::BeginNodeEditor();

        for (const Node& node : graph.nodes)
        {
            // 位置は、最初の 1 回だけ、グラフから渡す。そのあとは、ドラッグで動くので、imnodes が持つ
            if (impl_->placed.insert(node.id).second)
            {
                ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.posX, node.posY));
            }

            const bool isState = node.kind == NodeKind::State;
            const bool isStart = isState && node.id == graph.startNode;

            ImNodes::PushColorStyle(ImNodesCol_TitleBar, isState ? StateColor : ConditionColor);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, isState ? StateColorHover : ConditionColorHover);
            ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, isState ? StateColorHover : ConditionColorHover);
            if (isStart)
            {
                ImNodes::PushColorStyle(ImNodesCol_NodeOutline, StartOutlineColor);
                ImNodes::PushStyleVar(ImNodesStyleVar_NodeBorderThickness, 2.5f);
            }

            ImNodes::BeginNode(node.id);

            const NodeTypeInfo* info = FindNodeType(node.type);
            const std::string name = info ? info->displayName : node.type + "(未登録)";
            const std::string title = isStart ? "[開始] " + name : name;
            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(title.c_str());
            ImNodes::EndNodeTitleBar();

            ImNodes::BeginInputAttribute(InputPinId(node.id));
            ImGui::TextUnformatted("入力");
            ImNodes::EndInputAttribute();

            ImNodes::BeginOutputAttribute(OutputPinId(node.id));
            ImGui::TextUnformatted("出力");
            ImNodes::EndOutputAttribute();

            ImNodes::EndNode();

            if (isStart)
            {
                ImNodes::PopStyleVar();
                ImNodes::PopColorStyle();
            }
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
            ImNodes::PopColorStyle();
        }

        // 線:from の出力ピン → to の入力ピン。線の id は、並びの番号(線の追加・削除は B4-3 で、作り直す)
        for (size_t i = 0; i < graph.links.size(); ++i)
        {
            const Link& link = graph.links[i];
            ImNodes::Link(static_cast<int>(i) + 1, OutputPinId(link.from), InputPinId(link.to));
        }

        ImNodes::EndNodeEditor();

        // ドラッグで動いた位置を、グラフに書き戻す(保存したとき、位置が残る)
        bool changed = false;
        for (Node& node : graph.nodes)
        {
            const ImVec2 pos = ImNodes::GetNodeGridSpacePos(node.id);
            if (std::fabs(pos.x - node.posX) > 0.01f || std::fabs(pos.y - node.posY) > 0.01f)
            {
                node.posX = pos.x;
                node.posY = pos.y;
                changed = true;
            }
        }
        return changed;
    }
}
