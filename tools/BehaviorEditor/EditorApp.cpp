// 敵の行動の調整ツール KRBehaviorEditor
// 状態(青)と条件(緑)を、ノードでつないで、敵の行動のグラフを作る。詳しい使い方は docs/BehaviorEditor.md
//
// 今の段階(B4-7):窓・GraphEditor(部品)・はじめのグラフ(見本 = MakeStarterGraph)。ノードを置く・つなぐ・動かす・消すが、できる
// 次の段階:アセットのフォルダーを開く・一覧・保存・新規(B4-8〜)

#include <KR/KR.h>
#include <KR/Behavior/GraphEditor.h>
#include <KR/Behavior/NodeTypes.h>

#include <imgui.h>

#include <memory>

using namespace KR::Behavior;

class EditorApp : public KR::Application
{
public:
    EditorApp() : Application(KR::ApplicationDesc{ .title = "KR Behavior Editor (KR Engine " + KR::GetVersionString() + ")" }) {}

protected:
    void OnInit() override
    {
        m_Editor = std::make_unique<GraphEditor>();
    }

    void OnShutdown() override
    {
        m_Editor.reset();
    }

    void OnImGui() override
    {
        // 画面いっぱいの 1 枚の窓(タイトルバー・移動・大きさ変更なし)。メニューなどは、次の段階で、上に足す
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::Begin("グラフ", nullptr, flags);

        ImGui::TextDisabled("右クリック = ノードを足す / ノードの上で右クリック = 開始・消す / ピンからピンへドラッグ = 線 / 選んで Delete = 消す");
        m_Editor->DrawWarnings(m_Graph);
        m_Editor->Draw(m_Graph);

        ImGui::End();
    }

private:
    std::unique_ptr<GraphEditor> m_Editor;
    Graph m_Graph = MakeStarterGraph();
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<EditorApp>();
}
