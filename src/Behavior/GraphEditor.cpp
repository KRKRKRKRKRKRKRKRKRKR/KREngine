#include "KR/Behavior/GraphEditor.h"

#include "GraphEdit.h"
#include "KR/Behavior/NodeTypes.h"

#include <imgui.h>
#include <imnodes.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

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
        constexpr float FieldWidth = 130.0f; // 値の欄の幅

        // 登録(spec)から、値の欄を 1 つ出す。値の型は spec の既定値の型で決まる。変えたら true
        bool DrawParamField(ParamValue& value, const ParamSpec& spec)
        {
            const bool hasRange = spec.minValue != spec.maxValue;
            const char* label = spec.displayName.c_str();
            ImGui::SetNextItemWidth(FieldWidth);

            if (float* number = std::get_if<float>(&value))
            {
                const float speed = hasRange ? std::max((spec.maxValue - spec.minValue) / 500.0f, 0.01f) : 0.05f;
                return ImGui::DragFloat(label, number, speed, spec.minValue, spec.maxValue, "%.2f");
            }
            if (int* number = std::get_if<int>(&value))
            {
                return ImGui::DragInt(label, number, 0.2f, static_cast<int>(spec.minValue), static_cast<int>(spec.maxValue));
            }
            if (bool* flag = std::get_if<bool>(&value))
            {
                return ImGui::Checkbox(label, flag);
            }
            if (std::string* text = std::get_if<std::string>(&value))
            {
                char buffer[128];
                std::snprintf(buffer, sizeof(buffer), "%s", text->c_str());
                if (ImGui::InputText(label, buffer, sizeof(buffer)))
                {
                    *text = buffer;
                    return true;
                }
            }
            return false;
        }
    }

    struct GraphEditor::Impl
    {
        ImNodesContext* context = nullptr;
        std::set<NodeId> placed;       // 位置を、imnodes に渡した(= 以後は imnodes が持つ)ノード
        const Graph* lastGraph = nullptr;
        NodeId hoveredNode = 0;        // 直前のフレームで、マウスが乗っていたノード(右クリックで、どのメニューを出すかの判定)
        NodeId menuNode = 0;           // 「ノードのメニュー」を開いた、対象のノード
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

        bool changed = false;
        ImNodes::BeginNodeEditor();

        // 右クリックのメニュー:登録された種類の一覧から、ノードを足す(押した場所に置く)
        // ノードの上で右クリックしたときは、そのノードのメニュー(ここから始める・消す)。何もない所なら、足すメニュー
        const bool rightClicked = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImNodes::IsEditorHovered()
            && ImGui::IsMouseReleased(ImGuiMouseButton_Right);
        if (rightClicked && impl_->hoveredNode != 0)
        {
            impl_->menuNode = impl_->hoveredNode;
            ImGui::OpenPopup("ノードのメニュー");
        }
        else if (rightClicked && !ImGui::IsAnyItemHovered())
        {
            ImGui::OpenPopup("ノードを足す");
        }

        if (ImGui::BeginPopup("ノードのメニュー"))
        {
            const Node* target = graph.FindNode(impl_->menuNode);
            if (target == nullptr)
            {
                ImGui::CloseCurrentPopup();
            }
            else
            {
                const NodeId targetId = target->id;
                if (target->kind == NodeKind::State)
                {
                    const bool isStart = targetId == graph.startNode;
                    if (ImGui::MenuItem("ここから始める", nullptr, isStart, !isStart))
                    {
                        changed |= SetStartNode(graph, targetId);
                    }
                }
                else
                {
                    ImGui::TextDisabled("(条件は、開始にできない)");
                }
                if (ImGui::MenuItem("このノードを消す"))
                {
                    changed |= RemoveNode(graph, targetId);
                    impl_->placed.erase(targetId);
                }
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("ノードを足す"))
        {
            const ImVec2 clickPos = ImGui::GetMousePosOnOpeningCurrentPopup();
            const auto addItems = [&](NodeKind kind, const char* header)
            {
                ImGui::TextDisabled("%s", header);
                for (const NodeTypeInfo& type : GetNodeTypes())
                {
                    if (type.kind == kind && ImGui::MenuItem(type.displayName.c_str()))
                    {
                        const NodeId id = AddNode(graph, type.type, 0.0f, 0.0f);
                        ImNodes::SetNodeScreenSpacePos(id, clickPos);
                        impl_->placed.insert(id); // 位置は imnodes が持つ(下の書き戻しで、グラフに入る)
                        changed = true;
                    }
                }
            };
            addItems(NodeKind::State, "状態");
            ImGui::Separator();
            addItems(NodeKind::Condition, "条件");
            ImGui::EndPopup();
        }

        for (size_t nodeIndex = 0; nodeIndex < graph.nodes.size(); ++nodeIndex)
        {
            const Node& node = graph.nodes[nodeIndex];
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

            // 登録された値の欄(型ごと)。ノードごとに、ImGui の id を分ける(同じ名前の欄が、複数のノードにあるため)
            ImGui::PushID(node.id);
            if (info != nullptr)
            {
                Node& editable = graph.nodes[nodeIndex];
                for (const ParamSpec& spec : info->params)
                {
                    ParamValue& value = editable.params.try_emplace(spec.name, spec.defaultValue).first->second;
                    if (!spec.visibleWhen.empty())
                    {
                        const auto condition = editable.params.find(spec.visibleWhen);
                        const bool* shown = (condition != editable.params.end()) ? std::get_if<bool>(&condition->second) : nullptr;
                        if (shown == nullptr || !*shown)
                        {
                            continue;
                        }
                    }
                    ImGui::PushID(spec.name.c_str());
                    changed |= DrawParamField(value, spec);
                    ImGui::PopID();
                }
            }
            ImGui::PopID();

            // 分岐の順番:状態から出る線が 2 本以上のとき、調べる順(priority の小さい順)を、↑↓で入れ替える
            if (isState)
            {
                const std::vector<Link> outgoing = graph.OutgoingLinks(node.id);
                if (outgoing.size() >= 2)
                {
                    ImGui::PushID(node.id);
                    ImGui::TextDisabled("分岐の順番(上から調べる)");
                    for (size_t i = 0; i < outgoing.size(); ++i)
                    {
                        const Node* target = graph.FindNode(outgoing[i].to);
                        const NodeTypeInfo* targetInfo = target ? FindNodeType(target->type) : nullptr;
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::ArrowButton("##up", ImGuiDir_Up) && i > 0)
                        {
                            changed |= MoveLinkPriority(graph, node.id, outgoing[i].to, -1);
                        }
                        ImGui::SameLine();
                        if (ImGui::ArrowButton("##down", ImGuiDir_Down) && i + 1 < outgoing.size())
                        {
                            changed |= MoveLinkPriority(graph, node.id, outgoing[i].to, 1);
                        }
                        ImGui::SameLine();
                        ImGui::Text("%d. #%d %s", static_cast<int>(i) + 1, outgoing[i].to, targetInfo ? targetInfo->displayName.c_str() : "?");
                        ImGui::PopID();
                    }
                    ImGui::PopID();
                }
            }

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

        int hovered = 0;
        impl_->hoveredNode = ImNodes::IsNodeHovered(&hovered) ? hovered : 0;

        // 線の追加:ピンからピンへドラッグして離すと来る。出力ピン → 入力ピンの向きに直して、ルール(CanAddLink)に合うときだけ足す
        int startPin = 0;
        int endPin = 0;
        if (ImNodes::IsLinkCreated(&startPin, &endPin))
        {
            const bool startIsOutput = startPin % 4 == 2;
            const bool endIsOutput = endPin % 4 == 2;
            if (startIsOutput != endIsOutput) // 出力どうし・入力どうしは、つなげない
            {
                const int outputPin = startIsOutput ? startPin : endPin;
                const int inputPin = startIsOutput ? endPin : startPin;
                changed |= AddLink(graph, outputPin / 4, inputPin / 4);
            }
        }

        // 線の削除:線をピンから引き離した(IsLinkDestroyed)・選んで Delete キー。文字欄に入力中の Delete は、除く
        std::set<int> removeIndices;
        std::vector<int> selectedNodeIds;
        int destroyedLink = 0;
        if (ImNodes::IsLinkDestroyed(&destroyedLink))
        {
            removeIndices.insert(destroyedLink - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !ImGui::GetIO().WantTextInput)
        {
            const int selectedNodeCount = ImNodes::NumSelectedNodes();
            if (selectedNodeCount > 0)
            {
                selectedNodeIds.resize(static_cast<size_t>(selectedNodeCount));
                ImNodes::GetSelectedNodes(selectedNodeIds.data());
            }

            const int selectedCount = ImNodes::NumSelectedLinks();
            if (selectedCount > 0)
            {
                std::vector<int> selected(static_cast<size_t>(selectedCount));
                ImNodes::GetSelectedLinks(selected.data());
                for (const int id : selected)
                {
                    removeIndices.insert(id - 1);
                }
                ImNodes::ClearLinkSelection();
            }
        }
        for (auto it = removeIndices.rbegin(); it != removeIndices.rend(); ++it) // 後ろから消す(番号がずれない)
        {
            if (*it >= 0 && static_cast<size_t>(*it) < graph.links.size())
            {
                graph.links.erase(graph.links.begin() + *it);
                changed = true;
            }
        }

        // ノードの削除:選んで Delete キー(つながる線も消える)。線の削除のあとに行う(線の番号が、ずれるため)
        if (!selectedNodeIds.empty())
        {
            for (const int id : selectedNodeIds)
            {
                changed |= RemoveNode(graph, id);
                impl_->placed.erase(id);
            }
            ImNodes::ClearNodeSelection();
        }

        // ドラッグで動いた位置を、グラフに書き戻す(保存したとき、位置が残る)
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
