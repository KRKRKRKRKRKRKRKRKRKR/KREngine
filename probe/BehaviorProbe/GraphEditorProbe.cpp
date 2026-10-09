// GraphEditor(部品)の動作確認用:依頼の例のグラフ(待機 → 接近 → 移動 → 接近 → 攻撃)を、描く
// ツール KRBehaviorEditor(B4-7)ができたら、役目を終える

#include <KR/KR.h>
#include <KR/Behavior/GraphEditor.h>
#include <KR/Behavior/NodeTypes.h>

#include <imgui.h>

#include <memory>

using namespace KR::Behavior;

class GraphEditorProbe : public KR::Application
{
public:
    GraphEditorProbe() : Application(KR::ApplicationDesc{ .title = "KR GraphEditor Probe" }) {}

protected:
    void OnInit() override
    {
        m_Editor = std::make_unique<GraphEditor>();

        const auto add = [this](NodeId id, const char* type, float x, float y)
        {
            const NodeTypeInfo* info = FindNodeType(type);
            Node node;
            node.id = id;
            node.kind = info->kind;
            node.type = type;
            node.posX = x;
            node.posY = y;
            for (const ParamSpec& spec : info->params)
            {
                node.params[spec.name] = spec.defaultValue;
            }
            m_Graph.nodes.push_back(node);
        };
        add(1, "Wait", 20, 100);
        add(2, "PlayerWithin", 200, 100);
        add(3, "MoveToPlayer", 420, 100);
        add(4, "PlayerWithin", 640, 100);
        add(5, "Attack", 860, 100);
        add(6, "PlayerOutside", 420, 300); // 攻撃のあと、離れたら、待機へ戻る(例)
        m_Graph.startNode = 1;
        m_Graph.links = { { 1, 2, 0 }, { 2, 3, 0 }, { 3, 4, 0 }, { 4, 5, 0 }, { 5, 6, 0 }, { 6, 1, 0 } };
    }

    void OnImGui() override
    {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1240.0f, 560.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("GraphEditor");
        ImGui::Text("node1=(%.0f, %.0f)  nodes=%d  links=%d", m_Graph.nodes[0].posX, m_Graph.nodes[0].posY,
            static_cast<int>(m_Graph.nodes.size()), static_cast<int>(m_Graph.links.size()));
        m_Editor->Draw(m_Graph);
        ImGui::End();
    }

    void OnShutdown() override { m_Editor.reset(); }

private:
    std::unique_ptr<GraphEditor> m_Editor;
    Graph m_Graph;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<GraphEditorProbe>();
}
