// DrawGraphDebug の動作確認用:グラフで Runner を動かして、今の状態のノード・条件の真偽(緑 / 赤)が出るかを見る
// 使い方:KRDebugDrawProbe.exe [--graph <グラフの JSON>]  (無ければ、見本のグラフ)
// 上の窓 = 状況(距離・攻撃が終わったか)を入れる / 下の窓 = DrawGraphDebug

#include <KR/KR.h>

#include <imgui.h>

#include <memory>

using namespace KR::Behavior;

class DebugDrawProbe : public KR::Application
{
public:
    DebugDrawProbe() : Application(KR::ApplicationDesc{ .title = "KR DebugDraw Probe" }) {}

protected:
    void OnInit() override
    {
        const std::vector<std::filesystem::path> args = KR::GetCommandLineArguments();
        for (size_t i = 1; i + 1 < args.size(); ++i)
        {
            if (args[i] == L"--graph")
            {
                m_Graph = LoadGraph(args[i + 1]);
            }
        }
        if (m_Graph == nullptr)
        {
            m_Graph = std::make_shared<const Graph>(MakeStarterGraph());
        }
        m_Runner = std::make_unique<Runner>(m_Graph);
    }

    void OnUpdate(float deltaTime) override
    {
        Context context;
        context.SetFloat("distanceToPlayerX", m_Distance);
        context.SetFloat("distanceToPlayerY", 0.0f);
        context.SetBool("attackFinished", m_AttackFinished);
        m_Runner->Update(deltaTime, context);
    }

    void OnImGui() override
    {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1240.0f, 120.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("状況");
        ImGui::SliderFloat("プレイヤーまでの距離(横)", &m_Distance, 0.0f, 12.0f);
        ImGui::Checkbox("攻撃が終わった", &m_AttackFinished);
        const StateView state = m_Runner->GetCurrentState();
        ImGui::Text("今の状態: %s(#%d)  入ってから %.2f 秒  distance=%.2f  評価した条件 %d 個", state.GetType().c_str(), state.GetNodeId(),
            state.GetTimeInState(), m_Distance, static_cast<int>(m_Runner->GetLastConditionResults().size()));
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(10.0f, 140.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1240.0f, 540.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("DrawGraphDebug");
        DrawGraphDebug(*m_Runner);
        ImGui::End();
    }

    void OnShutdown() override
    {
        m_Runner.reset();
    }

private:
    GraphPtr m_Graph;
    std::unique_ptr<Runner> m_Runner;
    float m_Distance = 11.0f;
    bool m_AttackFinished = false;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<DebugDrawProbe>();
}
