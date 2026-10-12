// imnodes の動作確認(B3-2):ノード 2 つ + 線 1 本を、ImGui の窓に出す
// 確かめること:表示・ノードのドラッグ・ピンから線をつなぐ・線の削除・複数選択・日本語の文字

#include <KR/KR.h>

#include <imgui.h>
#include <imnodes.h>

#include <memory>
#include <utility>
#include <vector>

class ProbeApp : public KR::Application
{
public:
    ProbeApp() : Application(KR::ApplicationDesc{ .title = "KR Behavior Probe" }) {}

protected:
    void OnInit() override
    {
        ImNodes::CreateContext();
        ImNodes::SetNodeGridSpacePos(1, ImVec2(40.0f, 60.0f));
        ImNodes::SetNodeGridSpacePos(2, ImVec2(320.0f, 60.0f));
        m_Links.push_back({ 0, 11, 20 });
        m_NextLinkId = 1;
    }

    void OnShutdown() override
    {
        ImNodes::DestroyContext();
    }

    void OnImGui() override
    {
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(700.0f, 520.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Probe(imnodes)");

        // 状態の表示(目で見て確かめる・スクリーンショットで読む)。エディタの上に出す(下だと、エディタが場所を全部使って、切れる)
        ImGui::Text("node1=(%.0f, %.0f)  node2=(%.0f, %.0f)  links=%d  selectedNodes=%d  radius=%.1f",
            m_Pos1.x, m_Pos1.y, m_Pos2.x, m_Pos2.y, static_cast<int>(m_Links.size()), m_SelectedNodes, m_Radius);

        ImNodes::BeginNodeEditor();

        // ノード 1:出力ピン(id 11)。ピンの id は、ノードと重ならない数字にする
        ImNodes::BeginNode(1);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted("待機(Wait)");
        ImNodes::EndNodeTitleBar();
        ImNodes::BeginInputAttribute(12);
        ImGui::TextUnformatted("入力");
        ImNodes::EndInputAttribute();
        ImNodes::BeginOutputAttribute(11);
        ImGui::TextUnformatted("出力");
        ImNodes::EndOutputAttribute();
        ImNodes::EndNode();

        // ノード 2:入力ピン(id 20)・出力ピン(id 21)・値の欄
        ImNodes::BeginNode(2);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted("プレイヤーが半径以内");
        ImNodes::EndNodeTitleBar();
        ImNodes::BeginInputAttribute(20);
        ImGui::TextUnformatted("入力");
        ImNodes::EndInputAttribute();
        ImGui::PushItemWidth(120.0f);
        ImGui::DragFloat("半径", &m_Radius, 0.1f, 0.0f, 100.0f);
        ImGui::PopItemWidth();
        ImNodes::BeginOutputAttribute(21);
        ImGui::TextUnformatted("出力");
        ImNodes::EndOutputAttribute();
        ImNodes::EndNode();

        for (const Link& link : m_Links)
        {
            ImNodes::Link(link.id, link.from, link.to);
        }

        ImNodes::EndNodeEditor();
        m_Pos1 = ImNodes::GetNodeGridSpacePos(1);
        m_Pos2 = ImNodes::GetNodeGridSpacePos(2);
        m_SelectedNodes = ImNodes::NumSelectedNodes();

        // 新しい線:ピンから、ピンへドラッグして離すと、ここに来る
        int from = 0;
        int to = 0;
        if (ImNodes::IsLinkCreated(&from, &to))
        {
            m_Links.push_back({ m_NextLinkId++, from, to });
            KR_LOG_INFO("Probe: 線を作りました {} -> {}", from, to);
        }
        // 線の削除:線を選んで Delete キー
        int selectedLinks = ImNodes::NumSelectedLinks();
        if (selectedLinks > 0 && ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            std::vector<int> ids(static_cast<size_t>(selectedLinks));
            ImNodes::GetSelectedLinks(ids.data());
            for (int id : ids)
            {
                std::erase_if(m_Links, [id](const Link& link) { return link.id == id; });
            }
            ImNodes::ClearLinkSelection();
            KR_LOG_INFO("Probe: 線を {} 本消しました", selectedLinks);
        }

        ImGui::End();
    }

private:
    struct Link
    {
        int id;
        int from;
        int to;
    };

    std::vector<Link> m_Links;
    int m_NextLinkId = 1;
    float m_Radius = 3.0f;
    ImVec2 m_Pos1{};
    ImVec2 m_Pos2{};
    int m_SelectedNodes = 0;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<ProbeApp>();
}
