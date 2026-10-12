#include "KR/Behavior/GraphDebug.h"

#include "KR/Behavior/NodeTypes.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
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
        constexpr ImU32 BodyColor = IM_COL32(52, 52, 56, 255);
        constexpr ImU32 BorderColor = IM_COL32(95, 95, 100, 255);
        constexpr ImU32 ActiveColor = IM_COL32(255, 165, 40, 255);
        constexpr ImU32 LinkColor = IM_COL32(80, 120, 190, 200);
        constexpr ImU32 ActiveLinkColor = IM_COL32(255, 165, 40, 230);
        constexpr ImU32 TextColor = IM_COL32(235, 235, 235, 255);
        constexpr ImU32 SubTextColor = IM_COL32(185, 185, 190, 255);

        // 1 つのノードの、画面に出す文字(タイトル + 値の行)と、大きさ(縮小する前)
        struct NodeLayout
        {
            const Node* node = nullptr;
            std::string title;
            std::vector<std::string> lines;
            ImVec2 size;
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

        if (graph.nodes.empty())
        {
            draw->AddText(Add(origin, ImVec2(12.0f, 10.0f)), SubTextColor, "(ノードがありません)");
            draw->PopClipRect();
            return;
        }

        // ノードごとの文字と大きさ(縮小する前)
        const float lineHeight = ImGui::GetTextLineHeight();
        const float pad = 8.0f;
        std::vector<NodeLayout> layouts;
        layouts.reserve(graph.nodes.size());
        for (const Node& node : graph.nodes)
        {
            NodeLayout layout;
            layout.node = &node;
            const NodeTypeInfo* info = FindNodeType(node.type);
            layout.title = info ? info->displayName : node.type + "(未登録)";
            layout.lines = MakeLines(node, info);
            float textWidth = ImGui::CalcTextSize(layout.title.c_str()).x;
            for (const std::string& line : layout.lines)
            {
                textWidth = std::max(textWidth, ImGui::CalcTextSize(line.c_str()).x);
            }
            layout.size = ImVec2(std::max(textWidth + pad * 2.0f, 110.0f),
                lineHeight + pad * 2.0f + static_cast<float>(layout.lines.size()) * (lineHeight + 2.0f) + 4.0f);
            layouts.push_back(std::move(layout));
        }

        // グラフ全体の範囲 → 窓に収まる縮小率(拡大はしない)と、中央に置くずらし
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

        const auto rectOf = [&](const NodeLayout& layout) -> std::pair<ImVec2, ImVec2>
        {
            const ImVec2 topLeft(shift.x + layout.node->posX * safeScale, shift.y + layout.node->posY * safeScale);
            return { topLeft, ImVec2(topLeft.x + layout.size.x * safeScale, topLeft.y + layout.size.y * safeScale) };
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

        // 線:from の右中 → to の左中(ベジェ曲線 + 矢印)。今の状態から出る線は、橙
        for (const Link& link : graph.links)
        {
            const NodeLayout* from = findLayout(link.from);
            const NodeLayout* to = findLayout(link.to);
            if (from == nullptr || to == nullptr)
            {
                continue;
            }
            const auto [fromMin, fromMax] = rectOf(*from);
            const auto [toMin, toMax] = rectOf(*to);
            const ImVec2 start(fromMax.x, (fromMin.y + fromMax.y) * 0.5f);
            const ImVec2 end(toMin.x, (toMin.y + toMax.y) * 0.5f);
            const float reach = std::max(40.0f * safeScale, std::fabs(end.x - start.x) * 0.5f);
            const bool active = link.from == currentId;
            const ImU32 color = active ? ActiveLinkColor : LinkColor;
            const float thickness = active ? 2.6f : 1.6f;
            draw->AddBezierCubic(start, ImVec2(start.x + reach, start.y), ImVec2(end.x - reach, end.y), end, color, thickness);
            const float arrow = 6.0f;
            draw->AddTriangleFilled(end, ImVec2(end.x - arrow * 1.5f, end.y - arrow * 0.8f), ImVec2(end.x - arrow * 1.5f, end.y + arrow * 0.8f), color);
        }

        // ノード
        const float fontSize = ImGui::GetFontSize() * std::max(safeScale, 0.55f);
        ImFont* font = ImGui::GetFont();
        for (const NodeLayout& layout : layouts)
        {
            const auto [nodeMin, nodeMax] = rectOf(layout);
            const bool isState = layout.node->kind == NodeKind::State;
            const bool isCurrent = layout.node->id == currentId;
            const float rounding = 5.0f * safeScale;
            const float titleHeight = (lineHeight + pad) * safeScale;

            if (isCurrent)
            {
                // 光る:外側に、ぼかしの代わりの、広い半透明の枠 + 橙の太い枠
                draw->AddRectFilled(ImVec2(nodeMin.x - 6.0f, nodeMin.y - 6.0f), ImVec2(nodeMax.x + 6.0f, nodeMax.y + 6.0f), IM_COL32(255, 165, 40, 60), rounding + 5.0f);
            }
            draw->AddRectFilled(nodeMin, nodeMax, BodyColor, rounding);
            draw->AddRectFilled(nodeMin, ImVec2(nodeMax.x, nodeMin.y + titleHeight), isState ? StateColor : ConditionColor, rounding, ImDrawFlags_RoundCornersTop);
            draw->AddRect(nodeMin, nodeMax, isCurrent ? ActiveColor : BorderColor, rounding, 0, isCurrent ? 3.0f : 1.0f);

            draw->AddText(font, fontSize, ImVec2(nodeMin.x + pad * 0.6f * safeScale, nodeMin.y + pad * 0.4f * safeScale), TextColor, layout.title.c_str());
            float y = nodeMin.y + titleHeight + 3.0f * safeScale;
            for (const std::string& line : layout.lines)
            {
                draw->AddText(font, fontSize * 0.92f, ImVec2(nodeMin.x + pad * 0.6f * safeScale, y), SubTextColor, line.c_str());
                y += (lineHeight + 2.0f) * safeScale;
            }
        }

        // 左下:今の状態の名前と、入ってからの時間
        const NodeLayout* currentLayout = findLayout(currentId);
        char status[160] = {};
        std::snprintf(status, sizeof(status), "現在:%s(#%d)  入ってから %.2f 秒%s",
            currentLayout ? currentLayout->title.c_str() : "?", currentId, current.GetTimeInState(), current.EnteredThisFrame() ? "  ★今、入った" : "");
        draw->AddText(Add(origin, ImVec2(10.0f, area.y - lineHeight - 6.0f)), ActiveColor, status);

        draw->PopClipRect();
    }

    void DrawGraphDebug(const Runner& runner, float width, float height)
    {
        DrawGraphDebug(*runner.GetGraph(), runner, width, height);
    }
}
