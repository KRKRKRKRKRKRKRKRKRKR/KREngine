// GraphEditor(部品)の動作確認用:見本のグラフ(待機 → 接近 → 移動 → 接近 → 攻撃)を描く
// C5 で、GraphEditor を imgui-node-editor で書き直したあとの、確認に使う(今は、作り直し中の表示)

#include <KR/KR.h>
#include <KR/Behavior/GraphEditor.h>

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
        m_Graph = MakeStarterGraph();
        // 使い方:KRGraphEditorProbe.exe [--graph <グラフの JSON>](無ければ、見本のグラフ)
        const std::vector<std::filesystem::path> args = KR::GetCommandLineArguments();
        for (size_t i = 1; i + 1 < args.size(); ++i)
        {
            Graph loaded;
            if (args[i] == L"--observe")
            {
                m_Observed = std::stoi(args[i + 1].string()); // 光らせるノードの id(SetObservedNode の確認)。負の数 = 止まっている(灰色)
            }
            if (args[i] == L"--graph" && LoadGraphFile(args[i + 1], loaded) == GraphLoadResult::Loaded)
            {
                m_Graph = std::move(loaded);
            }
        }
    }

    void OnImGui() override
    {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1240.0f, 560.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("GraphEditor");
        ImGui::Text("edits=%d  nodes=%d  nextId=%d", m_Edits, static_cast<int>(m_Graph.nodes.size()), m_Graph.nextNodeId);
        m_Editor->DrawWarnings(m_Graph);
        m_Editor->SetObservedNode(m_Observed < 0 ? -m_Observed : m_Observed, m_Observed < 0);
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
    int m_Observed = 0;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<GraphEditorProbe>();
}
