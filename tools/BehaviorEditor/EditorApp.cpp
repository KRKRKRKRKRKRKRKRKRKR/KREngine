// 敵の行動の調整ツール KRBehaviorEditor
// 状態(青)と条件(緑)を、ノードでつないで、敵の行動のグラフを作る。詳しい使い方は docs/BehaviorEditor.md
//
// 今の段階(B4-9):フォルダーを開く・一覧・開く(B4-8)/ 保存・新規・読み直し(B4-9)
// 次の段階:「保存していない」の印・切り替え時の確認・設定の記憶(B4-10)

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
#include <string_view>
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

    // グラフの名前(ファイル名から .json を除いた物)として、使えるか。空・前後の空白や「.」・Windows で使えない文字(\ / : * ? " < > |)・制御文字は、不可
    // (Enemies フォルダーの外へ書くことになる「..」や「/」を、ここで防ぐ)
    bool IsValidGraphName(const std::string& name)
    {
        if (name.empty() || name.front() == ' ' || name.back() == ' ' || name.front() == '.' || name.back() == '.')
        {
            return false;
        }
        return std::none_of(name.begin(), name.end(), [](char c)
            { return static_cast<unsigned char>(c) < 0x20 || std::string_view("\\/:*?\"<>|").find(c) != std::string_view::npos; });
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
            m_StatusIsError = true;
            return false;
        }
        m_Graph = std::move(loaded);
        m_Editor->Reset(); // ノードの位置を、新しいグラフから、取り直す
        m_CurrentName = name;
        std::snprintf(m_NameText, sizeof(m_NameText), "%s", name.c_str());
        m_StatusMessage = "";
        m_StatusIsError = true;
        return true;
    }

    // ---- 保存・新規・読み直し ----

    // 名前 = 保存先(Enemies/<名前>.json)。今の名前と違う名前で、同じ名前のファイルが既にあるときは、上書きの確認を出す(confirmed = 確認が済んだ)
    bool SaveGraphAs(const std::string& name, bool confirmed)
    {
        if (!IsValidGraphName(name))
        {
            m_StatusMessage = "名前が使えません(空・前後の空白や「.」・\\ / : * ? \" < > | は、使えません)";
            m_StatusIsError = true;
            return false;
        }

        const std::filesystem::path file = KR::GetAssetDirectory(EnemiesFolderName) / FromUtf8(name + ".json");
        std::error_code error;
        if (!confirmed && name != m_CurrentName && std::filesystem::exists(file, error))
        {
            m_PendingOverwriteName = name;
            m_OpenOverwritePopup = true;
            return false;
        }

        if (!SaveGraph(m_Graph, file))
        {
            m_StatusMessage = "保存できません: " + name + ".json(ログを見てください)";
            m_StatusIsError = true;
            return false;
        }
        m_CurrentName = name;
        RefreshFileList(); // 新しい名前を、一覧に出す(この中で、状態の行は、空になる)
        m_StatusMessage = "保存しました: " + name + ".json";
        m_StatusIsError = false;
        return true;
    }

    // 新規:待機 1 つの、まっさらなグラフ。名前は、まだ無い(保存のとき、付ける)
    void NewGraph()
    {
        m_Graph = *MakeDefaultGraph();
        if (Node* start = m_Graph.FindNode(m_Graph.startNode))
        {
            start->posX = 40.0f;
            start->posY = 60.0f;
        }
        m_Editor->Reset();
        m_CurrentName.clear();
        m_NameText[0] = '\0';
        m_StatusMessage = "";
        m_StatusIsError = true;
    }

    // 読み直し:今の名前のファイルを、読み直す(今のグラフの変更は、捨てられる)
    void ReloadGraph()
    {
        if (m_CurrentName.empty())
        {
            m_StatusMessage = "まだ保存していないので、読み直せません";
            m_StatusIsError = true;
            return;
        }
        OpenGraph(m_CurrentName);
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

        // 保存・新規・読み直し。名前を変えて保存すると、別のファイルになる
        ImGui::SeparatorText("このグラフ");
        ImGui::SetNextItemWidth(-1.0f);
        const bool nameEntered = ImGui::InputTextWithHint("##name", "名前(.json は、不要)", m_NameText, sizeof(m_NameText), ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::Button("保存") || nameEntered)
        {
            SaveGraphAs(m_NameText, false);
        }
        ImGui::SameLine();
        if (ImGui::Button("新規"))
        {
            NewGraph();
        }
        ImGui::SameLine();
        if (ImGui::Button("読み直し"))
        {
            ReloadGraph();
        }

        // Ctrl+S = 保存(文字欄に入力中は、除く)
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S) && !ImGui::GetIO().WantTextInput)
        {
            SaveGraphAs(m_NameText, false);
        }

        if (m_OpenOverwritePopup)
        {
            ImGui::OpenPopup("上書きの確認");
            m_OpenOverwritePopup = false;
        }
        if (ImGui::BeginPopupModal("上書きの確認", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("「%s.json」は、すでにあります。上書きしますか?", m_PendingOverwriteName.c_str());
            if (ImGui::Button("上書きする"))
            {
                SaveGraphAs(m_PendingOverwriteName, true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("やめる"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SeparatorText("敵の行動(Enemies)");
        if (ImGui::Button("一覧を更新"))
        {
            RefreshFileList();
        }

        ImGui::BeginChild("一覧", ImVec2(0.0f, -110.0f), ImGuiChildFlags_Borders);
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
            ImGui::PushStyleColor(ImGuiCol_Text, m_StatusIsError ? ImVec4(1.0f, 0.75f, 0.25f, 1.0f) : ImVec4(0.45f, 0.85f, 0.55f, 1.0f));
            ImGui::TextWrapped("%s", m_StatusMessage.c_str());
            ImGui::PopStyleColor();
        }
    }

    std::unique_ptr<GraphEditor> m_Editor;
    Graph m_Graph = MakeStarterGraph();

    std::filesystem::path m_Folder;     // アセットのフォルダー(KR::SetAssetRoot に渡した物)
    char m_FolderText[512] = {};        // 文字欄(UTF-8)
    std::vector<std::string> m_Files;   // Enemies/ の下の、グラフの名前(.json を除く。UTF-8)
    std::string m_CurrentName;          // 開いているグラフの名前(空 = 見本・まだ保存していない)
    std::string m_StatusMessage;        // 画面に出す 1 行(エラーなど)
    bool m_StatusIsError = true;        // 1 行の色(エラー = 橙・成功 = 緑)
    char m_NameText[128] = {};          // 保存する名前の文字欄(UTF-8。.json を除く)
    std::string m_PendingOverwriteName; // 上書きの確認の、待ち
    bool m_OpenOverwritePopup = false;
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<EditorApp>();
}
