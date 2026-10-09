// 敵の行動の調整ツール KRBehaviorEditor
// 状態(青)と条件(緑)を、ノードでつないで、敵の行動のグラフを作る。詳しい使い方は docs/BehaviorEditor.md
//
// 今の段階(B4-8):アセットのフォルダーを開く(文字欄・引数 --assets)・Assets/Enemies/*.json の一覧・選んで開く
// 次の段階:保存・新規・読み直し(B4-9)・「保存していない」の印・設定の記憶(B4-10)

#include <KR/KR.h>
#include <KR/Behavior/GraphEditor.h>
#include <KR/Behavior/GraphFile.h>
#include <KR/Behavior/NodeTypes.h>

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace KR::Behavior;

namespace
{
    constexpr const char* EnemiesFolderName = "Enemies"; // アセットのフォルダーの下の、敵の行動のグラフを置く場所

    // path ⇔ UTF-8 の文字列(ImGui の文字欄・画面の表示は UTF-8。path(const char*) は、日本語 Windows で、CP932 として読まれて、例外になる)
    std::string ToUtf8(const std::filesystem::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char*>(text.data()), text.size());
    }

    std::filesystem::path FromUtf8(const std::string& text)
    {
        return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
    }
}

class EditorApp : public KR::Application
{
public:
    EditorApp() : Application(KR::ApplicationDesc{ .title = "KR Behavior Editor (KR Engine " + KR::GetVersionString() + ")" }) {}

protected:
    void OnInit() override
    {
        m_Editor = std::make_unique<GraphEditor>();

        // 最初のフォルダー:引数 --assets <フォルダー> があればそれ。無ければ、ビルドで決まっているゲームのアセットのフォルダー
        std::filesystem::path folder = KR::GetAssetDirectory(EnemiesFolderName).parent_path();
        const std::vector<std::filesystem::path> args = KR::GetCommandLineArguments();
        for (size_t i = 1; i + 1 < args.size(); ++i)
        {
            if (args[i] == L"--assets")
            {
                folder = args[i + 1];
            }
        }
        std::snprintf(m_FolderText, sizeof(m_FolderText), "%s", ToUtf8(folder).c_str());
        ApplyFolder(folder);
    }

    void OnShutdown() override
    {
        m_Editor.reset();
    }

    void OnImGui() override
    {
        // 画面いっぱいの 1 枚の窓(タイトルバー・移動・大きさ変更なし)。左 = フォルダーと一覧 / 右 = グラフ
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::Begin("グラフ", nullptr, flags);

        ImGui::BeginChild("左", ImVec2(PanelWidth, 0.0f), ImGuiChildFlags_Borders);
        DrawProjectSection();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("右クリック = ノードを足す / ノードの上で右クリック = 開始・消す / ピンからピンへドラッグ = 線 / 選んで Delete = 消す");
        m_Editor->DrawWarnings(m_Graph);
        m_Editor->Draw(m_Graph);
        ImGui::EndGroup();

        ImGui::End();
    }

private:
    static constexpr float PanelWidth = 280.0f;

    // ---- フォルダーと一覧 ----

    // アセットのフォルダーを決める:KR::SetAssetRoot で、エンジンの読み書きの場所を切り替え、Enemies/ の下の .json を、一覧にする
    // フォルダーが無ければ、断る(false)。Enemies/ が、まだ無いのは、よい(保存すると、作られる)
    bool ApplyFolder(const std::filesystem::path& folder)
    {
        std::error_code error;
        if (folder.empty() || !std::filesystem::is_directory(folder, error))
        {
            m_StatusMessage = "フォルダーがありません: " + ToUtf8(folder);
            KR_LOG_WARN("BehaviorEditor: フォルダーがありません: {}", ToUtf8(folder));
            return false;
        }

        KR::SetAssetRoot(folder);
        m_Folder = folder;
        std::snprintf(m_FolderText, sizeof(m_FolderText), "%s", ToUtf8(folder).c_str());
        RefreshFileList();
        KR_LOG_INFO("BehaviorEditor: アセットのフォルダー {}(グラフ {} 個)", ToUtf8(folder), m_Files.size());
        return true;
    }

    // Enemies/ の下の *.json(直下だけ)を、名前順に、一覧にする(.json を除いた名前)
    void RefreshFileList()
    {
        m_Files.clear();
        const std::filesystem::path directory = KR::GetAssetDirectory(EnemiesFolderName);
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error))
        {
            m_StatusMessage = "Enemies フォルダーは、まだありません(保存すると、作られます)";
            return;
        }
        for (const auto& entry : std::filesystem::directory_iterator(directory, std::filesystem::directory_options::skip_permission_denied, error))
        {
            if (entry.is_regular_file() && entry.path().extension() == L".json")
            {
                m_Files.push_back(ToUtf8(entry.path().stem()));
            }
        }
        std::sort(m_Files.begin(), m_Files.end());
        m_StatusMessage = m_Files.empty() ? "Enemies フォルダーに、.json がありません" : "";
    }

    // 一覧の 1 つを開く(名前 = .json を除いた名前)。読めなければ、今のグラフのまま、理由を出す
    bool OpenGraph(const std::string& name)
    {
        const std::filesystem::path file = KR::GetAssetDirectory(EnemiesFolderName) / FromUtf8(name + ".json");
        Graph loaded;
        const GraphLoadResult result = LoadGraphFile(file, loaded);
        if (result != GraphLoadResult::Loaded)
        {
            m_StatusMessage = (result == GraphLoadResult::NotFound ? "ファイルがありません: " : "読み込めません(壊れています): ") + name + ".json";
            return false;
        }
        m_Graph = std::move(loaded);
        m_Editor->Reset(); // ノードの位置を、新しいグラフから、取り直す
        m_CurrentName = name;
        m_StatusMessage = "";
        return true;
    }

    // ---- ImGui の各区画 ----

    void DrawProjectSection()
    {
        ImGui::SeparatorText("アセットのフォルダー");

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##folder", m_FolderText, sizeof(m_FolderText), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            ApplyFolder(FromUtf8(m_FolderText)); // Enter で開く
        }
        ImGui::TextDisabled("(Enter で開く)");

        ImGui::SeparatorText("敵の行動(Enemies)");
        if (ImGui::Button("一覧を更新"))
        {
            RefreshFileList();
        }

        ImGui::BeginChild("一覧", ImVec2(0.0f, -80.0f), ImGuiChildFlags_Borders);
        for (const std::string& name : m_Files)
        {
            if (ImGui::Selectable(name.c_str(), name == m_CurrentName))
            {
                OpenGraph(name);
            }
        }
        ImGui::EndChild();

        ImGui::Text("開いているグラフ:");
        ImGui::TextWrapped("%s", m_CurrentName.empty() ? "(見本のグラフ)" : m_CurrentName.c_str());
        if (!m_StatusMessage.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "%s", m_StatusMessage.c_str());
        }
    }

    std::unique_ptr<GraphEditor> m_Editor;
    Graph m_Graph = MakeStarterGraph();

    std::filesystem::path m_Folder;     // アセットのフォルダー(KR::SetAssetRoot に渡した物)
    char m_FolderText[512] = {};        // 文字欄(UTF-8)
    std::vector<std::string> m_Files;   // Enemies/ の下の、グラフの名前(.json を除く。UTF-8)
    std::string m_CurrentName;          // 開いているグラフの名前(空 = 見本・まだ保存していない)
    std::string m_StatusMessage;        // 画面に出す 1 行(エラーなど)
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<EditorApp>();
}
