#include "KR/Behavior/GraphDebug.h"

#include "KR/Behavior/NodeTypes.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace KR::Behavior
{
    namespace
    {
        constexpr ImU32 BackgroundColor = IM_COL32(28, 29, 35, 255);
        constexpr ImU32 StateColor = IM_COL32(50, 100, 190, 255);
        constexpr ImU32 ConditionColor = IM_COL32(40, 150, 90, 255);
        constexpr ImU32 LogicColor = IM_COL32(120, 95, 170, 255);
        constexpr ImU32 BodyColor = IM_COL32(52, 52, 56, 255);
        constexpr ImU32 BorderColor = IM_COL32(95, 95, 100, 255);
        constexpr ImU32 ActiveColor = IM_COL32(255, 165, 40, 255);
        constexpr ImU32 FlowColor = IM_COL32(210, 210, 215, 170);         // 流れ(遷移の行 → 次の状態)
        constexpr ImU32 ActiveFlowColor = IM_COL32(255, 165, 40, 235);
        constexpr ImU32 BoolUnknownColor = IM_COL32(150, 70, 70, 150);    // 真偽の線:まだ評価していない
        constexpr ImU32 BoolTrueColor = IM_COL32(80, 220, 110, 235);      // 真
        constexpr ImU32 BoolFalseColor = IM_COL32(215, 70, 70, 235);      // 偽
        constexpr ImU32 TextColor = IM_COL32(235, 235, 235, 255);
        constexpr ImU32 SubTextColor = IM_COL32(185, 185, 190, 255);

        // 1 つのノードの、画面に出す文字と、大きさ・ピンの位置(縮小する前。ノードの左上からの距離)
        struct NodeLayout
        {
            const Node* node = nullptr;
            const NodeTypeInfo* info = nullptr;
            std::string title;
            std::vector<std::string> lines; // 値の行
            std::vector<std::string> rows;  // 状態:遷移の行の文字 / 論理の条件:入力の文字
            ImVec2 size;
            float titleHeight = 0.0f;
            float firstRowY = 0.0f;
            float rowHeight = 0.0f;
        };

        // 値の行:小数・整数は「名前 値」。真偽は、true のときだけ「名前」。文字(キー名)は、出さない(じゃまになる)
        std::vector<std::string> MakeLines(const Node& node, const NodeTypeInfo* info)
        {
            std::vector<std::string> lines;
            if (info == nullptr)
            {
                return lines;
            }
            for (const ParamSpec& spec : info->params)
            {
                const auto found = node.params.find(spec.name);
                if (found == node.params.end())
                {
                    continue;
                }
                char text[96] = {};
                if (const float* number = std::get_if<float>(&found->second))
                {
                    std::snprintf(text, sizeof(text), "%s %.2f", spec.displayName.c_str(), *number);
                }
                else if (const int* integer = std::get_if<int>(&found->second))
                {
                    std::snprintf(text, sizeof(text), "%s %d", spec.displayName.c_str(), *integer);
                }
                else if (const bool* flag = std::get_if<bool>(&found->second); flag != nullptr && *flag)
                {
                    std::snprintf(text, sizeof(text), "%s", spec.displayName.c_str());
                }
                if (text[0] != '\0')
                {
                    lines.emplace_back(text);
                }
            }
            return lines;
        }

        ImVec2 Add(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
    }

    void DrawGraphDebug(const Graph& graph, const Runner& runner, float width, float height)
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 area(std::max(width > 0.0f ? width : available.x, 80.0f), std::max(height > 0.0f ? height : available.y, 80.0f));
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(area);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 areaEnd = Add(origin, area);
        draw->AddRectFilled(origin, areaEnd, BackgroundColor, 4.0f);
        draw->PushClipRect(origin, areaEnd, true);

        const StateView current = runner.GetCurrentState();
        const NodeId currentId = current.GetNodeId();
        const std::vector<ConditionResult>& results = runner.GetLastConditionResults();
        // 直前の Update の、条件の結果:0 = 偽 / 1 = 真 / -1 = 評価していない
        const auto resultOf = [&](NodeId id) -> int
        {
            for (const ConditionResult& r : results)
            {
                if (r.node == id)
                {
                    return r.value ? 1 : 0;
                }
            }
            return -1;
        };

        if (graph.nodes.empty())
        {
            draw->AddText(Add(origin, ImVec2(12.0f, 10.0f)), SubTextColor, "(ノードがありません)");
            draw->PopClipRect();
            return;
        }

        // ---- 今の状態の行が、使っている条件(入力も、たどる)= 太い線で出す ----
        std::set<NodeId> activeConditions;
        if (const Node* currentNode = graph.FindNode(currentId))
        {
            std::vector<NodeId> pending;
            for (const Transition& row : currentNode->transitions)
            {
                if (row.condition != 0)
                {
                    pending.push_back(row.condition);
                }
            }
            while (!pending.empty())
            {
                const NodeId id = pending.back();
                pending.pop_back();
                if (!activeConditions.insert(id).second)
                {
                    continue;
                }
                if (const Node* node = graph.FindNode(id))
                {
                    for (const NodeId input : node->inputs)
                    {
                        if (input != 0)
                        {
                            pending.push_back(input);
                        }
                    }
                }
            }
        }

        // ---- ノードごとの文字と大きさ(縮小する前)----
        const float lineHeight = ImGui::GetTextLineHeight();
        const float pad = 8.0f;
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

        std::vector<NodeLayout> layouts;
        layouts.reserve(graph.nodes.size());
        for (const Node& node : graph.nodes)
        {
            NodeLayout layout;
            layout.node = &node;
            layout.info = FindNodeType(node.type);
            layout.title = layout.info ? layout.info->displayName : node.type + "(未登録)";
            layout.lines = MakeLines(node, layout.info);
            if (node.kind == NodeKind::State)
            {
                for (size_t i = 0; i < node.transitions.size(); ++i)
                {
                    const Transition& row = node.transitions[i];
                    layout.rows.push_back(std::to_string(i + 1) + ". " + (row.to != 0 ? "→ " + nameOf(row.to) : "(次の状態なし)"));
                }
            }
            else if (layout.info != nullptr && layout.info->IsLogic())
            {
                for (size_t i = 0; i < node.inputs.size(); ++i)
                {
                    layout.rows.push_back("入力 " + std::to_string(i + 1));
                }
            }
            float textWidth = ImGui::CalcTextSize(layout.title.c_str()).x;
            for (const std::string& line : layout.lines)
            {
                textWidth = std::max(textWidth, ImGui::CalcTextSize(line.c_str()).x);
            }
            for (const std::string& row : layout.rows)
            {
                textWidth = std::max(textWidth, ImGui::CalcTextSize(row.c_str()).x + 28.0f); // 左右のピンの分
            }
            layout.titleHeight = lineHeight + pad;
            layout.rowHeight = lineHeight + 4.0f;
            layout.firstRowY = layout.titleHeight + 3.0f + static_cast<float>(layout.lines.size()) * (lineHeight + 2.0f);
            layout.size = ImVec2(std::max(textWidth + pad * 2.0f + 14.0f, 120.0f),
                layout.firstRowY + static_cast<float>(layout.rows.size()) * layout.rowHeight + pad * 0.5f);
            layouts.push_back(std::move(layout));
        }

        // ---- グラフ全体の範囲 → 窓に収まる縮小率(拡大はしない)と、中央に置くずらし ----
        float minX = layouts[0].node->posX;
        float minY = layouts[0].node->posY;
        float maxX = minX;
        float maxY = minY;
        for (const NodeLayout& layout : layouts)
        {
            minX = std::min(minX, layout.node->posX);
            minY = std::min(minY, layout.node->posY);
            maxX = std::max(maxX, layout.node->posX + layout.size.x);
            maxY = std::max(maxY, layout.node->posY + layout.size.y);
        }
        const float margin = 16.0f;
        const float footer = lineHeight + 14.0f; // 左下の文字の場所
        const float boundsWidth = std::max(maxX - minX, 1.0f);
        const float boundsHeight = std::max(maxY - minY, 1.0f);
        const float scale = std::min({ (area.x - margin * 2.0f) / boundsWidth, (area.y - margin * 2.0f - footer) / boundsHeight, 1.0f });
        const float safeScale = std::max(scale, 0.2f);
        const ImVec2 shift(origin.x + (area.x - boundsWidth * safeScale) * 0.5f - minX * safeScale,
                           origin.y + (area.y - footer - boundsHeight * safeScale) * 0.5f - minY * safeScale);

        const auto topLeftOf = [&](const NodeLayout& layout) -> ImVec2
        {
            return ImVec2(shift.x + layout.node->posX * safeScale, shift.y + layout.node->posY * safeScale);
        };
        const auto findLayout = [&](NodeId id) -> const NodeLayout*
        {
            for (const NodeLayout& layout : layouts)
            {
                if (layout.node->id == id)
                {
                    return &layout;
                }
            }
            return nullptr;
        };
        // ピンの位置:行ごとの、左(入力)・右(出力)/ 状態の「入」= タイトルの左 / 条件の「出」= タイトルの右
        const auto titlePin = [&](const NodeLayout& layout, bool right) -> ImVec2
        {
            const ImVec2 topLeft = topLeftOf(layout);
            return ImVec2(right ? topLeft.x + layout.size.x * safeScale : topLeft.x, topLeft.y + layout.titleHeight * 0.5f * safeScale);
        };
        const auto rowPin = [&](const NodeLayout& layout, size_t row, bool right) -> ImVec2
        {
            const ImVec2 topLeft = topLeftOf(layout);
            return ImVec2(right ? topLeft.x + layout.size.x * safeScale : topLeft.x,
                topLeft.y + (layout.firstRowY + (static_cast<float>(row) + 0.5f) * layout.rowHeight) * safeScale);
        };
        const auto wire = [&](ImVec2 start, ImVec2 end, ImU32 color, float thickness)
        {
            const float reach = std::max(40.0f * safeScale, std::fabs(end.x - start.x) * 0.5f);
            draw->AddBezierCubic(start, ImVec2(start.x + reach, start.y), ImVec2(end.x - reach, end.y), end, color, thickness);
        };

        // ---- 線(ノードの下に描く)----
        const float arrow = 6.0f;
        for (const NodeLayout& layout : layouts)
        {
            const Node& node = *layout.node;
            if (node.kind == NodeKind::State)
            {
                const bool isCurrent = node.id == currentId;
                for (size_t i = 0; i < node.transitions.size(); ++i)
                {
                    const Transition& row = node.transitions[i];
                    // 真偽の線:条件の「出」→ この行の、左のピン
                    if (row.condition != 0)
                    {
                        if (const NodeLayout* source = findLayout(row.condition))
                        {
                            const int value = resultOf(row.condition);
                            const ImU32 color = value < 0 ? BoolUnknownColor : (value == 1 ? BoolTrueColor : BoolFalseColor);
                            wire(titlePin(*source, true), rowPin(layout, i, false), color, isCurrent ? 2.4f : 1.2f);
                        }
                    }
                    // 流れの線:この行の、右のピン → 次の状態の「入」
                    if (row.to != 0)
                    {
                        if (const NodeLayout* target = findLayout(row.to))
                        {
                            const ImVec2 end = titlePin(*target, false);
                            const ImU32 color = isCurrent ? ActiveFlowColor : FlowColor;
                            wire(rowPin(layout, i, true), end, color, isCurrent ? 2.8f : 1.6f);
                            draw->AddTriangleFilled(end, ImVec2(end.x - arrow * 1.5f, end.y - arrow * 0.8f), ImVec2(end.x - arrow * 1.5f, end.y + arrow * 0.8f), color);
                        }
                    }
                }
            }
            else
            {
                // 論理の条件:入力の条件の「出」→ この論理の、行(入力)の左のピン
                for (size_t i = 0; i < node.inputs.size(); ++i)
                {
                    if (node.inputs[i] == 0)
                    {
                        continue;
                    }
                    if (const NodeLayout* source = findLayout(node.inputs[i]))
                    {
                        const int value = resultOf(node.inputs[i]);
                        const ImU32 color = value < 0 ? BoolUnknownColor : (value == 1 ? BoolTrueColor : BoolFalseColor);
                        wire(titlePin(*source, true), rowPin(layout, i, false), color, activeConditions.contains(node.id) ? 2.0f : 1.2f);
                    }
                }
            }
        }

        // ---- ノード ----
        const float fontSize = ImGui::GetFontSize() * std::max(safeScale, 0.55f);
        ImFont* font = ImGui::GetFont();
        const float pinRadius = 3.5f * std::max(safeScale, 0.6f);
        for (const NodeLayout& layout : layouts)
        {
            const Node& node = *layout.node;
            const ImVec2 nodeMin = topLeftOf(layout);
            const ImVec2 nodeMax(nodeMin.x + layout.size.x * safeScale, nodeMin.y + layout.size.y * safeScale);
            const bool isState = node.kind == NodeKind::State;
            const bool isLogic = layout.info != nullptr && layout.info->IsLogic();
            const bool isCurrent = node.id == currentId;
            const int value = isState ? -1 : resultOf(node.id);
            const float rounding = 5.0f * safeScale;
            const float titleHeight = layout.titleHeight * safeScale;

            if (isCurrent)
            {
                // 光る:外側に、ぼかしの代わりの、広い半透明の枠 + 橙の太い枠
                draw->AddRectFilled(ImVec2(nodeMin.x - 6.0f, nodeMin.y - 6.0f), ImVec2(nodeMax.x + 6.0f, nodeMax.y + 6.0f), IM_COL32(255, 165, 40, 60), rounding + 5.0f);
            }
            draw->AddRectFilled(nodeMin, nodeMax, BodyColor, rounding);
            draw->AddRectFilled(nodeMin, ImVec2(nodeMax.x, nodeMin.y + titleHeight), isState ? StateColor : (isLogic ? LogicColor : ConditionColor), rounding, ImDrawFlags_RoundCornersTop);
            ImU32 border = BorderColor;
            float borderWidth = 1.0f;
            if (isCurrent)
            {
                border = ActiveColor;
                borderWidth = 3.0f;
            }
            else if (value >= 0)
            {
                border = value == 1 ? BoolTrueColor : BoolFalseColor; // 条件:今、真 = 緑の枠 / 偽 = 赤の枠
                borderWidth = 2.0f;
            }
            draw->AddRect(nodeMin, nodeMax, border, rounding, 0, borderWidth);

            draw->AddText(font, fontSize, ImVec2(nodeMin.x + pad * 0.6f * safeScale, nodeMin.y + pad * 0.4f * safeScale), TextColor, layout.title.c_str());
            if (!isState && value >= 0)
            {
                // 条件の結果:タイトルの右端に「真」「偽」
                const char* label = value == 1 ? "真" : "偽";
                const float labelWidth = ImGui::CalcTextSize(label).x * std::max(safeScale, 0.55f);
                draw->AddText(font, fontSize, ImVec2(nodeMax.x - labelWidth - pad * 0.6f * safeScale, nodeMin.y + pad * 0.4f * safeScale), value == 1 ? BoolTrueColor : BoolFalseColor, label);
            }
            float y = nodeMin.y + titleHeight + 3.0f * safeScale;
            for (const std::string& line : layout.lines)
            {
                draw->AddText(font, fontSize * 0.92f, ImVec2(nodeMin.x + pad * 0.6f * safeScale, y), SubTextColor, line.c_str());
                y += (lineHeight + 2.0f) * safeScale;
            }
            for (size_t i = 0; i < layout.rows.size(); ++i)
            {
                const ImVec2 left = rowPin(layout, i, false);
                draw->AddText(font, fontSize * 0.92f, ImVec2(left.x + 10.0f * safeScale, left.y - lineHeight * 0.5f * safeScale), SubTextColor, layout.rows[i].c_str());
                draw->AddCircleFilled(left, pinRadius, BoolUnknownColor); // 真偽の入口(左)
                if (isState)
                {
                    draw->AddCircleFilled(rowPin(layout, i, true), pinRadius, isCurrent ? ActiveColor : FlowColor); // 流れの出口(右)
                }
            }
            // タイトルの、左右のピン:状態 = 流れの入口(左)/ 条件 = 真偽の出口(右)
            if (isState)
            {
                draw->AddCircleFilled(titlePin(layout, false), pinRadius, FlowColor);
            }
            else
            {
                draw->AddCircleFilled(titlePin(layout, true), pinRadius, value < 0 ? BoolUnknownColor : (value == 1 ? BoolTrueColor : BoolFalseColor));
            }
        }

        // ---- 左下:今の状態の名前と、入ってからの時間 / 右下:色の説明 ----
        const NodeLayout* currentLayout = findLayout(currentId);
        char status[160] = {};
        std::snprintf(status, sizeof(status), "現在:%s(#%d)  入ってから %.2f 秒%s",
            currentLayout ? currentLayout->title.c_str() : "?", currentId, current.GetTimeInState(), current.EnteredThisFrame() ? "  ★今、入った" : "");
        draw->AddText(Add(origin, ImVec2(10.0f, area.y - lineHeight - 6.0f)), ActiveColor, status);
        const char* legend = "条件:緑 = 今、真 / 赤 = 偽 / 橙の線 = 今の状態の遷移";
        const float legendWidth = ImGui::CalcTextSize(legend).x;
        draw->AddText(ImVec2(areaEnd.x - legendWidth - 10.0f, area.y + origin.y - lineHeight - 6.0f), SubTextColor, legend);

        draw->PopClipRect();
    }

    void DrawGraphDebug(const Runner& runner, float width, float height)
    {
        DrawGraphDebug(*runner.GetGraph(), runner, width, height);
    }
}
