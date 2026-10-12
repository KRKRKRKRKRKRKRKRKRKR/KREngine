// DrawGraphDebug の動作確認用:見本のグラフで Runner を動かして、今の状態のノードが光るかを見る
// 上の窓 = 状況(距離・攻撃が終わったか)を入れる / 下の窓 = DrawGraphDebug
// サンプル KRBehavior(B6-1)ができたら、役目を終える

#include <KR/KR.h>
#include <KR/Behavior/Context.h>
#include <KR/Behavior/GraphDebug.h>
#include <KR/Behavior/Runner.h>

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
        m_Graph = std::make_shared<const Graph>(MakeStarterGraph());
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
        ImGui::SliderFloat("プレイヤーまでの距離(横)", &m_Distance, 0.0f, 10.0f);
        ImGui::Checkbox("攻撃が終わった", &m_AttackFinished);
        const StateView state = m_Runner->GetCurrentState();
        ImGui::Text("今の状態: %s  入ってから %.2f 秒  distance=%.2f", state.GetType().c_str(), state.GetTimeInState(), m_Distance);
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(10.0f, 140.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1240.0f, 480.0f), ImGuiCond_FirstUseEver);
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
    float m_Distance = 10.0f;
    bool m_AttackFinished = false;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<DebugDrawProbe>();
}
