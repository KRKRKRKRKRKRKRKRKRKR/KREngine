// GraphEditor(部品)の動作確認用:依頼の例のグラフ(待機 → 接近 → 移動 → 接近 → 攻撃)を、描く
// ツール KRBehaviorEditor(B4-7)ができたら、役目を終える

#include <KR/KR.h>
#include <KR/Behavior/GraphEditor.h>
#include <KR/Behavior/NodeTypes.h>

#include <imgui.h>

#include <memory>
#include <string>

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
        add(2, "PlayerWithin", 170, 100);
        add(3, "MoveToPlayer", 440, 100);
        add(4, "PlayerWithin", 640, 100);
        add(5, "Attack", 930, 100);
        add(6, "PlayerOutside", 440, 330); // 攻撃のあと、離れたら、待機へ戻る(例)
        m_Graph.startNode = 1;
        m_Graph.links = { { 1, 2, 0 }, { 2, 3, 0 }, { 3, 4, 0 }, { 4, 5, 0 }, { 5, 6, 0 }, { 6, 1, 0 } };
    }

    void OnImGui() override
    {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1240.0f, 560.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("GraphEditor");
        // 状態の表示。ノードは、並びの番号ではなく id で探す(削除で並びが変わるため)。無い・型が違うときは、「-」
        const auto number = [this](NodeId id, const char* name) -> std::string
        {
            const Node* node = m_Graph.FindNode(id);
            if (node == nullptr)
            {
                return "-";
            }
            const auto found = node->params.find(name);
            if (found == node->params.end())
            {
                return "-";
            }
            if (const float* f = std::get_if<float>(&found->second)) { return std::to_string(*f); }
            if (const int* i = std::get_if<int>(&found->second)) { return std::to_string(*i); }
            if (const bool* b = std::get_if<bool>(&found->second)) { return *b ? "true" : "false"; }
            return "-";
        };
        ImGui::Text("edits=%d  radius(id2)=%s  vertical(id2)=%s  damage(id5)=%s  nodes=%d  links=%d  nextId=%d", m_Edits,
            number(2, "radius").c_str(), number(2, "includeVertical").c_str(), number(5, "damage").c_str(),
            static_cast<int>(m_Graph.nodes.size()), static_cast<int>(m_Graph.links.size()), m_Graph.nextNodeId);
        if (m_Editor->Draw(m_Graph))
        {
            ++m_Edits;
        }
        ImGui::End();
    }

    void OnShutdown() override { m_Editor.reset(); }

private:
    std::unique_ptr<GraphEditor> m_Editor;
    Graph m_Graph;
    int m_Edits = 0;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<GraphEditorProbe>();
}
