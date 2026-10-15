// GraphEditor(ノードエディタの部品)
// imgui-node-editor で描く(ズームあり。見た目は、UE のブループリント風)。imnodes から替えた経緯は docs/Behavior.md の 15 節
//   C5a(ここ):表示(状態・条件・論理の条件・遷移の行ごとの別のピン・線)・ズーム・移動・ノードのドラッグ・値の欄・開始ノードの印・SetObservedNode・警告の一覧
//   C5b:線をつなぐ・消す(規則つき)・右クリックのメニュー・Delete  /  C5c:遷移の行の「＋」「▲▼」「✕」・論理の入力の「＋」

#include "KR/Behavior/GraphEditor.h"

#include "GraphEdit.h"
#include "KR/Behavior/GraphValidate.h"
#include "KR/Behavior/NodeTypes.h"

#include <imgui.h>
#include <imgui_node_editor.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace ed = ax::NodeEditor;

namespace KR::Behavior
{
    namespace
    {
        // ---- 色 ----
        constexpr ImU32 StateColor = IM_COL32(50, 100, 190, 255);
        constexpr ImU32 ConditionColor = IM_COL32(40, 150, 90, 255);
        constexpr ImU32 LogicColor = IM_COL32(120, 95, 170, 255);
        constexpr ImU32 StartOutlineColor = IM_COL32(255, 160, 40, 255);
        constexpr ImU32 ObservedColor = IM_COL32(255, 165, 40, 255);
        constexpr ImU32 ObservedPausedColor = IM_COL32(150, 150, 155, 255);
        constexpr ImU32 FlowPinColor = IM_COL32(235, 235, 240, 255);   // 流れのピン・線(UE の実行の線のように、白)
        constexpr ImU32 BoolPinColor = IM_COL32(210, 65, 65, 255);     // 真偽のピン・線(UE の bool のように、赤)
        constexpr ImU32 TextColor = IM_COL32(235, 235, 235, 255);
        constexpr ImU32 DimTextColor = IM_COL32(165, 165, 172, 255);

        constexpr float FieldWidth = 130.0f; // 値の欄の幅
        constexpr float RowWidth = 250.0f;   // 遷移の行・論理の入力の行の、幅(左右のピンを、そろえるため)
        constexpr float PinIconSize = 14.0f;

        ImVec4 ToVec4(ImU32 color) { return ImGui::ColorConvertU32ToFloat4(color); }

        // ---- id(imgui-node-editor の、ノード・ピン・線の番号)----
        // ノードの番号 = グラフのノードの id(小さい整数)。ピン = (ノード id << 20) | (種類 << 16) | 番号(行・入力の番号)= ノードの番号と、重ならない。
        // 線の番号 = 目的のピン(真偽)/ 出発のピン(流れ)から作る(ピンごとに、線は 1 本まで = 番号が、毎フレーム、同じ)
        enum class PinType : std::uintptr_t
        {
            FlowIn = 1,         // 状態の「入」(流れの入口)
            RowCondition = 2,   // 遷移の行の、条件の入口(真偽。左)
            RowFlowOut = 3,     // 遷移の行の、次の状態への出口(流れ。右)
            ConditionOut = 4,   // 条件のノードの、出口(真偽)
            LogicInput = 5,     // かつ・または・でない の、入力(真偽。左)
        };

        std::uintptr_t MakePinId(NodeId node, PinType type, int index = 0)
        {
            return (static_cast<std::uintptr_t>(node) << 20) | (static_cast<std::uintptr_t>(type) << 16) | static_cast<std::uintptr_t>(index & 0xFFFF);
        }
        std::uintptr_t BoolLinkId(std::uintptr_t destinationPin) { return (std::uintptr_t(1) << 62) | destinationPin; }
        std::uintptr_t FlowLinkId(std::uintptr_t sourcePin) { return (std::uintptr_t(1) << 61) | sourcePin; }

        // ---- 部品 ----
        // ピンの丸:つながっていれば塗る・つながっていなければ、輪だけ。ImGui の部品として、場所をとる(ed::BeginPin の中で呼ぶ)
        void DrawPinIcon(bool connected, ImU32 color)
        {
            ImGui::Dummy(ImVec2(PinIconSize, PinIconSize));
            const ImVec2 minimum = ImGui::GetItemRectMin();
            const ImVec2 maximum = ImGui::GetItemRectMax();
            const ImVec2 center((minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f);
            ImDrawList* draw = ImGui::GetWindowDrawList();
            if (connected)
            {
                draw->AddCircleFilled(center, 5.0f, color);
            }
            else
            {
                draw->AddCircle(center, 5.0f, color, 0, 1.8f);
            }
        }

        // ピンを置く:入力 = 丸 + 文字 / 出力 = 文字 + 丸。リンクは、ピンの左端(入力)・右端(出力)の、まん中につながる
        void BeginPinAt(std::uintptr_t id, bool isInput)
        {
            ed::BeginPin(ed::PinId(id), isInput ? ed::PinKind::Input : ed::PinKind::Output);
            ed::PinPivotAlignment(ImVec2(isInput ? 0.0f : 1.0f, 0.5f));
            ed::PinPivotSize(ImVec2(0.0f, 0.0f));
        }

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
        ed::EditorContext* context = nullptr;
        std::set<NodeId> placed;           // 位置を、imgui-node-editor に渡した(= 以後は、そちらが持つ)ノード
        const Graph* lastGraph = nullptr;
        int navigateFrames = 0;            // > 0 の間、数えて、0 になったフレームで、全体が見えるように、画面を合わせる(ノードの大きさが、分かってから)
        NodeId observedNode = 0;           // 光らせるノード(ゲームの敵の今の状態。0 = 光らせない)
        bool observedPaused = false;       // ゲームが止まっている(灰色で光らせる)
    };

    GraphEditor::GraphEditor()
        : impl_(std::make_unique<Impl>())
    {
        ed::Config config;
        config.SettingsFile = nullptr; // 位置・拡大の保存ファイル(NodeEditor.json)を、作らない(位置は、グラフが持つ)
        // 拡大縮小の段階(ホイール 1 目盛りで、次の段へ)。値は「拡大率の逆数」:小さいほど、拡大。ライブラリの既定(0.1〜10)は、極端なので、狭める
        for (const float level : { 0.33f, 0.4f, 0.45f, 0.5f, 0.55f, 0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.0f, 1.05f, 1.1f, 1.15f, 1.2f, 1.3f, 1.4f, 1.5f, 1.65f, 1.8f, 2.0f, 2.25f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f })
        {
            config.CustomZoomLevels.push_back(level);
        }
        impl_->context = ed::CreateEditor(&config);
    }

    GraphEditor::~GraphEditor()
    {
        ed::DestroyEditor(impl_->context);
    }

    void GraphEditor::Reset()
    {
        impl_->placed.clear();
        impl_->lastGraph = nullptr;
        impl_->navigateFrames = 3;
    }

    // 他の担当が足した機能(ツールの、動いている敵の今の状態を光らせる = Observe)
    void GraphEditor::SetObservedNode(NodeId node, bool paused)
    {
        impl_->observedNode = node;
        impl_->observedPaused = paused;
    }

    bool GraphEditor::Draw(Graph& graph)
    {
        ed::SetCurrentEditor(impl_->context);
        if (impl_->lastGraph != &graph)
        {
            impl_->placed.clear(); // 別のグラフ
            impl_->lastGraph = &graph;
            impl_->navigateFrames = 3;
        }

        bool changed = false;
        ed::Begin("グラフのエディタ");

        const ed::Style& style = ed::GetStyle();
        const auto nameOf = [&](NodeId id) -> std::string
        {
            const Node* node = graph.FindNode(id);
            if (node == nullptr)
            {
                return "?";
            }
            const NodeTypeInfo* info = FindNodeType(node->type);
            return info ? info->displayName : node->type;
        };

        // どの条件・状態が、つながっているか(ピンを、塗るか)
        std::set<NodeId> conditionsInUse;
        std::set<NodeId> statesTargeted;
        for (const Node& node : graph.nodes)
        {
            for (const Transition& row : node.transitions)
            {
                if (row.condition != 0)
                {
                    conditionsInUse.insert(row.condition);
                }
                if (row.to != 0)
                {
                    statesTargeted.insert(row.to);
                }
            }
            for (const NodeId input : node.inputs)
            {
                if (input != 0)
                {
                    conditionsInUse.insert(input);
                }
            }
        }

        // ---- ノード ----
        for (size_t nodeIndex = 0; nodeIndex < graph.nodes.size(); ++nodeIndex)
        {
            Node& node = graph.nodes[nodeIndex];
            const ed::NodeId key(static_cast<std::uintptr_t>(node.id));
            const NodeTypeInfo* info = FindNodeType(node.type);
            const bool isState = node.kind == NodeKind::State;
            const bool isLogic = info != nullptr && info->IsLogic();
            const bool isStart = isState && node.id == graph.startNode;
            const bool isObserved = impl_->observedNode != 0 && node.id == impl_->observedNode;
            const ImU32 observedColor = impl_->observedPaused ? ObservedPausedColor : ObservedColor;

            // 位置は、最初の 1 回だけ、グラフから渡す。そのあとは、ドラッグで動くので、imgui-node-editor が持つ
            if (impl_->placed.insert(node.id).second)
            {
                ed::SetNodePosition(key, ImVec2(node.posX, node.posY));
            }

            // 開始ノード = 橙の太い枠 / 光らせるノード = 橙(止まっていれば、灰色)の太い枠
            const bool outlined = isStart || isObserved;
            if (outlined)
            {
                ed::PushStyleColor(ed::StyleColor_NodeBorder, ToVec4(isObserved ? observedColor : StartOutlineColor));
                ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, isObserved ? 3.5f : 2.5f);
            }

            ed::BeginNode(key);
            ImGui::PushID(node.id);
            ImGui::BeginGroup(); // ノード全体

            // 題の行:状態 = [流れの入口の丸] 名前 / 条件 = 名前 ....... 成立 [真偽の出口の丸]
            ImGui::BeginGroup();
            const std::string title = (isStart ? "[開始] " : "") + nameOf(node.id);
            if (isState)
            {
                BeginPinAt(MakePinId(node.id, PinType::FlowIn), true);
                DrawPinIcon(statesTargeted.contains(node.id), FlowPinColor);
                ed::EndPin();
                ImGui::SameLine();
                ImGui::TextUnformatted(title.c_str());
            }
            else
            {
                ImGui::TextUnformatted(title.c_str());
                const float titleWidth = ImGui::CalcTextSize(title.c_str()).x;
                const float labelWidth = ImGui::CalcTextSize("成立").x;
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(std::max(RowWidth - titleWidth - labelWidth - PinIconSize - 12.0f, 8.0f), 1.0f));
                ImGui::SameLine();
                BeginPinAt(MakePinId(node.id, PinType::ConditionOut), false);
                ImGui::TextColored(ToVec4(DimTextColor), "成立");
                ImGui::SameLine();
                DrawPinIcon(conditionsInUse.contains(node.id), BoolPinColor);
                ed::EndPin();
            }
            ImGui::EndGroup();
            const ImVec2 headerMin = ImGui::GetItemRectMin();
            const ImVec2 headerMax = ImGui::GetItemRectMax();

            // 値の欄(登録から、自動)
            if (info != nullptr)
            {
                for (const ParamSpec& spec : info->params)
                {
                    ParamValue& value = node.params.try_emplace(spec.name, spec.defaultValue).first->second;
                    if (!spec.visibleWhen.empty())
                    {
                        const auto condition = node.params.find(spec.visibleWhen);
                        const bool* shown = (condition != node.params.end()) ? std::get_if<bool>(&condition->second) : nullptr;
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

            // 状態:遷移の行(左 = 条件の入口 / 右 = 次の状態の出口。行ごとに、別のピン)
            if (isState)
            {
                if (!node.transitions.empty())
                {
                    ImGui::Dummy(ImVec2(1.0f, 2.0f));
                }
                for (size_t i = 0; i < node.transitions.size(); ++i)
                {
                    const Transition& row = node.transitions[i];
                    const int index = static_cast<int>(i);
                    ImGui::PushID(index);
                    const std::string right = row.to != 0 ? "→ " + nameOf(row.to) : "→ 次の状態";
                    const float rightWidth = ImGui::CalcTextSize(right.c_str()).x + PinIconSize + 4.0f;
                    const float leftWidth = PinIconSize + 4.0f + ImGui::CalcTextSize("もし 条件").x;

                    BeginPinAt(MakePinId(node.id, PinType::RowCondition, index), true);
                    DrawPinIcon(row.condition != 0, BoolPinColor);
                    ImGui::SameLine(0.0f, 4.0f);
                    ImGui::TextColored(ToVec4(row.condition != 0 ? TextColor : DimTextColor), "もし 条件");
                    ed::EndPin();
                    ImGui::SameLine();
                    ImGui::Dummy(ImVec2(std::max(RowWidth - leftWidth - rightWidth - 16.0f, 8.0f), 1.0f));
                    ImGui::SameLine();
                    BeginPinAt(MakePinId(node.id, PinType::RowFlowOut, index), false);
                    ImGui::TextColored(ToVec4(row.to != 0 ? TextColor : DimTextColor), "%s", right.c_str());
                    ImGui::SameLine(0.0f, 4.0f);
                    DrawPinIcon(row.to != 0, FlowPinColor);
                    ed::EndPin();
                    ImGui::PopID();
                }
            }

            // 論理の条件:入力の行(左 = 真偽の入口)
            if (isLogic)
            {
                for (size_t i = 0; i < node.inputs.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    BeginPinAt(MakePinId(node.id, PinType::LogicInput, static_cast<int>(i)), true);
                    DrawPinIcon(node.inputs[i] != 0, BoolPinColor);
                    ImGui::SameLine(0.0f, 4.0f);
                    ImGui::TextColored(ToVec4(node.inputs[i] != 0 ? TextColor : DimTextColor), "入力 %d", static_cast<int>(i) + 1);
                    ed::EndPin();
                    ImGui::PopID();
                }
            }

            ImGui::EndGroup();
            const ImVec2 contentMin = ImGui::GetItemRectMin();
            const ImVec2 contentMax = ImGui::GetItemRectMax();
            ImGui::PopID();
            ed::EndNode();
            if (outlined)
            {
                ed::PopStyleVar();
                ed::PopStyleColor();
            }

            // 題の背景の色(状態 = 青・条件 = 緑・論理 = 紫。光らせるノードは、橙 / 灰色)。ノードの背景の上に、描く
            if (ImDrawList* background = ed::GetNodeBackgroundDrawList(key))
            {
                const ImU32 headerColor = isObserved ? observedColor : (isState ? StateColor : (isLogic ? LogicColor : ConditionColor));
                const ImVec4 padding = style.NodePadding;
                background->AddRectFilled(ImVec2(contentMin.x - padding.x, headerMin.y - padding.y), ImVec2(contentMax.x + padding.z, headerMax.y + 3.0f),
                    headerColor, style.NodeRounding, ImDrawFlags_RoundCornersTop);
            }
        }

        // ---- 線 ----
        const ImVec4 flowLinkColor = ToVec4(FlowPinColor);
        const ImVec4 boolLinkColor = ToVec4(BoolPinColor);
        for (const Node& node : graph.nodes)
        {
            const bool fromObserved = impl_->observedNode != 0 && node.id == impl_->observedNode;
            const ImVec4 activeColor = ToVec4(impl_->observedPaused ? ObservedPausedColor : ObservedColor);
            for (size_t i = 0; i < node.transitions.size(); ++i)
            {
                const Transition& row = node.transitions[i];
                const int index = static_cast<int>(i);
                if (row.condition != 0 && graph.FindNode(row.condition) != nullptr)
                {
                    const std::uintptr_t destination = MakePinId(node.id, PinType::RowCondition, index);
                    ed::Link(ed::LinkId(BoolLinkId(destination)), ed::PinId(MakePinId(row.condition, PinType::ConditionOut)), ed::PinId(destination), boolLinkColor, 2.0f);
                }
                if (row.to != 0 && graph.FindNode(row.to) != nullptr)
                {
                    const std::uintptr_t source = MakePinId(node.id, PinType::RowFlowOut, index);
                    ed::Link(ed::LinkId(FlowLinkId(source)), ed::PinId(source), ed::PinId(MakePinId(row.to, PinType::FlowIn)),
                        fromObserved ? activeColor : flowLinkColor, fromObserved ? 3.0f : 2.0f);
                }
            }
            for (size_t i = 0; i < node.inputs.size(); ++i)
            {
                if (node.inputs[i] != 0 && graph.FindNode(node.inputs[i]) != nullptr)
                {
                    const std::uintptr_t destination = MakePinId(node.id, PinType::LogicInput, static_cast<int>(i));
                    ed::Link(ed::LinkId(BoolLinkId(destination)), ed::PinId(MakePinId(node.inputs[i], PinType::ConditionOut)), ed::PinId(destination), boolLinkColor, 2.0f);
                }
            }
        }

        // 読み込んだ(開いた)直後は、全体が見えるように、画面を合わせる(ノードの大きさが、1 フレーム分かってから)
        if (impl_->navigateFrames > 0 && --impl_->navigateFrames == 0)
        {
            ed::NavigateToContent(); // 既定の時間(短い動き)で、画面を合わせる
        }

        // ドラッグで動いた位置を、グラフに書き戻す(保存したとき、位置が残る)
        for (Node& node : graph.nodes)
        {
            const ImVec2 position = ed::GetNodePosition(ed::NodeId(static_cast<std::uintptr_t>(node.id)));
            if (std::fabs(position.x - node.posX) > 0.01f || std::fabs(position.y - node.posY) > 0.01f)
            {
                node.posX = position.x;
                node.posY = position.y;
                changed = true;
            }
        }

        ed::End();
        ed::SetCurrentEditor(nullptr);
        return changed;
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
