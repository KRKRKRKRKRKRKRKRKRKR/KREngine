// imgui-node-editor の動作確認用(C0。ゲート):今の ImGui(v1.92.9b)で、動くか
// 確かめること:表示・日本語・ノードのドラッグ・ピンから線をつなぐ・線の削除・**ホイールでズーム**・右クリックのメニュー・設定ファイルを作らない
// 動かなければ、止めて相談する(計画書の決まり)

#include <KR/KR.h>

#include <imgui.h>
#include <imgui_node_editor.h>

#include <memory>
#include <string>
#include <vector>

namespace ed = ax::NodeEditor;

class NodeEditorProbe : public KR::Application
{
public:
    NodeEditorProbe() : Application(KR::ApplicationDesc{ .title = "KR NodeEditor Probe" }) {}

protected:
    void OnInit() override
    {
        ed::Config config;
        config.SettingsFile = nullptr; // 位置・拡大の保存ファイル(NodeEditor.json)を、作らない(位置は、グラフが持つ)
        m_Context = ed::CreateEditor(&config);
        m_Links.push_back({ 1, 12, 21 }); // 状態 1 の出力 → 条件 2 の入力
    }

    void OnShutdown() override
    {
        ed::DestroyEditor(m_Context);
        m_Context = nullptr;
    }

    void OnImGui() override
    {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1000.0f, 560.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("imgui-node-editor");

        ed::SetCurrentEditor(m_Context);
        ImGui::Text("zoom=%.2f  links=%d  nodes: A=(%.0f,%.0f) B=(%.0f,%.0f)  context menu: %s", m_Zoom, static_cast<int>(m_Links.size()),
            m_PosA.x, m_PosA.y, m_PosB.x, m_PosB.y, m_Menu.c_str());
        ed::Begin("エディタ");

        if (m_First)
        {
            ed::SetNodePosition(1, ImVec2(40.0f, 60.0f));
            ed::SetNodePosition(2, ImVec2(380.0f, 60.0f));
            m_First = false;
        }

        // ノード 1(青い見出しの代わりに、文字):出力ピン 12
        ed::BeginNode(1);
        ImGui::TextUnformatted("巡回(状態)");
        ed::BeginPin(11, ed::PinKind::Input);
        ImGui::TextUnformatted("● 入");
        ed::EndPin();
        ImGui::SameLine(0.0f, 80.0f);
        ed::BeginPin(12, ed::PinKind::Output);
        ImGui::TextUnformatted("出 ●");
        ed::EndPin();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::DragFloat("速さ", &m_Speed, 0.05f, 0.0f, 20.0f);
        ed::EndNode();

        // ノード 2:入力ピン 21・出力ピン 22・値の欄
        ed::BeginNode(2);
        ImGui::TextUnformatted("プレイヤーが半径以内(条件)");
        ed::BeginPin(21, ed::PinKind::Input);
        ImGui::TextUnformatted("● 入");
        ed::EndPin();
        ImGui::SameLine(0.0f, 120.0f);
        ed::BeginPin(22, ed::PinKind::Output);
        ImGui::TextUnformatted("成立 ●");
        ed::EndPin();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::DragFloat("半径", &m_Radius, 0.05f, 0.0f, 100.0f);
        ed::EndNode();

        for (const Link& link : m_Links)
        {
            ed::Link(link.id, link.from, link.to);
        }

        // 線をつなぐ:ピンから、ピンへドラッグ。出力 → 入力だけ許可(それ以外は、拒否の表示)
        if (ed::BeginCreate())
        {
            ed::PinId a, b;
            if (ed::QueryNewLink(&a, &b))
            {
                const bool aIsOutput = a.Get() % 10 == 2;
                const bool bIsOutput = b.Get() % 10 == 2;
                if (a && b && a != b && aIsOutput != bIsOutput)
                {
                    if (ed::AcceptNewItem())
                    {
                        const uintptr_t from = aIsOutput ? a.Get() : b.Get();
                        const uintptr_t to = aIsOutput ? b.Get() : a.Get();
                        m_Links.push_back({ m_NextLink++, from, to });
                    }
                }
                else
                {
                    ed::RejectNewItem(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 2.0f);
                }
            }
        }
        ed::EndCreate(); // BeginCreate が false でも、必ず呼ぶ

        // 削除:線を選んで Delete
        if (ed::BeginDelete())
        {
            ed::LinkId deleted;
            while (ed::QueryDeletedLink(&deleted))
            {
                if (ed::AcceptDeletedItem())
                {
                    std::erase_if(m_Links, [&](const Link& link) { return link.id == deleted.Get(); });
                }
            }
        }
        ed::EndDelete(); // BeginDelete が false でも、必ず呼ぶ

        // 右クリックのメニュー(ズームしても、窓の中の文字が、普通の大きさで出る = Suspend / Resume)
        ed::Suspend();
        ed::NodeId contextNode;
        if (ed::ShowNodeContextMenu(&contextNode))
        {
            m_Menu = "node";
            ImGui::OpenPopup("nodeMenu");
        }
        else if (ed::ShowBackgroundContextMenu())
        {
            m_Menu = "background";
            ImGui::OpenPopup("bgMenu");
        }
        if (ImGui::BeginPopup("nodeMenu"))
        {
            ImGui::MenuItem("ここから始める");
            ImGui::MenuItem("このノードを消す");
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("bgMenu"))
        {
            ImGui::TextDisabled("状態");
            ImGui::MenuItem("待機");
            ImGui::MenuItem("巡回");
            ImGui::EndPopup();
        }
        ed::Resume();

        m_Zoom = ed::GetCurrentZoom();
        m_PosA = ed::GetNodePosition(1);
        m_PosB = ed::GetNodePosition(2);
        ed::End();
        ed::SetCurrentEditor(nullptr);
        ImGui::End();
    }

private:
    struct Link
    {
        uintptr_t id;
        uintptr_t from;
        uintptr_t to;
    };

    ed::EditorContext* m_Context = nullptr;
    std::vector<Link> m_Links;
    uintptr_t m_NextLink = 100;
    bool m_First = true;
    float m_Speed = 1.5f;
    float m_Radius = 3.0f;
    float m_Zoom = 1.0f;
    ImVec2 m_PosA{};
    ImVec2 m_PosB{};
    std::string m_Menu = "-";
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<NodeEditorProbe>();
}
