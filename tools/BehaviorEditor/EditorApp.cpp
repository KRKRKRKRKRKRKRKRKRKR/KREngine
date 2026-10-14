// 敵の行動の調整ツール KRBehaviorEditor
// 状態(青)と条件(緑)を、ノードでつないで、敵の行動のグラフを作る。詳しい使い方は docs/BehaviorEditor.md
//
// 今の段階(B4-10):フォルダーを開く・一覧・開く / 保存・新規・読み直し / 保存していない印・確認の窓・設定の記憶・引数(--assets・--graph)

#include <KR/KR.h>
#include <KR/Behavior/GraphEditor.h>
#include <KR/Behavior/GraphFile.h>
#include <KR/Behavior/NodeTypes.h>

#include "FolderDialog.h" // tools/Common/(調整ツールどうしで共有)

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace KR::Behavior;

namespace
{
    constexpr const char* EnemiesFolderName = "Enemies"; // アセットのフォルダーの下の、敵の行動のグラフを置く場所
    constexpr size_t MaxRecentFolders = 8;               // 「最近...」に残すフォルダーの数

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
    // enableEditorKey = false:ツール自身では、F1 で自分を開かない(ゲームの F1 で開かれる側)
    EditorApp() : Application(KR::ApplicationDesc{ .title = "KR Behavior Editor (KR Engine " + KR::GetVersionString() + ")", .enableEditorKey = false }) {}

protected:
    void OnInit() override
    {
        m_Editor = std::make_unique<GraphEditor>();
        LoadSettings();

        // 最初のフォルダー:引数 --assets <フォルダー> → 前回のフォルダー(設定)→ ビルドで決まっているゲームのアセットのフォルダー
        // 最初のグラフ:引数 --graph <名前> → (前回と同じフォルダーのときだけ)前回のグラフ。無ければ、見本
        std::filesystem::path folder = m_SavedFolder.empty() ? KR::GetAssetDirectory(EnemiesFolderName).parent_path() : m_SavedFolder;
        std::string graphName = m_SavedGraph;
        bool folderFromArgs = false;
        bool graphFromArgs = false;
        const std::vector<std::filesystem::path> args = KR::GetCommandLineArguments();
        for (size_t i = 1; i + 1 < args.size(); ++i)
        {
            if (args[i] == L"--assets")
            {
                folder = args[i + 1];
                folderFromArgs = true;
            }
            else if (args[i] == L"--graph")
            {
                graphName = ToUtf8(args[i + 1]);
                graphFromArgs = true;
            }
        }
        if (graphName.size() > 5 && graphName.substr(graphName.size() - 5) == ".json")
        {
            graphName.resize(graphName.size() - 5); // --graph Walker.json でも、Walker でも、よい
        }

        std::snprintf(m_FolderText, sizeof(m_FolderText), "%s", ToUtf8(folder).c_str());
        if (ApplyFolder(folder) && !graphName.empty() && (graphFromArgs || !folderFromArgs))
        {
            OpenGraph(graphName);
        }
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
        if (m_Editor->Draw(m_Graph))
        {
            m_Dirty = true; // ノードを動かした・値を変えた・線やノードを足した / 消した
        }
        ImGui::EndGroup();

        ImGui::End();
    }

private:
    static constexpr float PanelWidth = 280.0f;

    // ---- 保存していない変更の確認 ----
    // フォルダーを変える・別のグラフを開く・新規・読み直しは、今のグラフの変更を捨てることになる。変更があるときは、確認の窓を出して、選んでもらう

    enum class ActionKind { Folder, Open, New, Reload };
    struct PendingAction
    {
        ActionKind kind = ActionKind::New;
        std::string text; // Folder = フォルダー(UTF-8)/ Open = グラフの名前
    };

    void RequestAction(const PendingAction& action)
    {
        if (m_Dirty)
        {
            m_Pending = action;
            m_OpenConfirmPopup = true;
            return;
        }
        PerformAction(action);
    }

    void PerformAction(const PendingAction& action)
    {
        switch (action.kind)
        {
        case ActionKind::Folder:
            if (ApplyFolder(FromUtf8(action.text)))
            {
                NewGraph(); // 別のフォルダーでは、前のフォルダーのグラフを、持ち越さない
            }
            break;
        case ActionKind::Open: OpenGraph(action.text); break;
        case ActionKind::New: NewGraph(); break;
        case ActionKind::Reload: ReloadGraph(); break;
        }
    }

    // 確認の窓(「保存して続ける」「捨てて続ける」「やめる」)
    void DrawConfirmPopup()
    {
        if (m_OpenConfirmPopup)
        {
            ImGui::OpenPopup("保存していない変更");
            m_OpenConfirmPopup = false;
        }
        if (!ImGui::BeginPopupModal("保存していない変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        ImGui::Text("「%s」に、保存していない変更があります。", m_CurrentName.empty() ? "(名前のないグラフ)" : m_CurrentName.c_str());
        if (ImGui::Button("保存して続ける"))
        {
            // 名前のないグラフは、名前の文字欄の名前で保存する(使えない名前なら、続けずに、理由を出す)
            if (SaveGraphAs(m_CurrentName.empty() ? std::string(m_NameText) : m_CurrentName, true))
            {
                PerformAction(m_Pending);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("捨てて続ける"))
        {
            m_Dirty = false;
            PerformAction(m_Pending);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("やめる"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- 設定の記憶(exe の隣の KRBehaviorEditor.txt。1 行 1 項目。UTF-8)----

    std::filesystem::path GetSettingsFile() const
    {
        const std::vector<std::filesystem::path> args = KR::GetCommandLineArguments();
        return (args.empty() ? std::filesystem::path() : args.front().parent_path()) / "KRBehaviorEditor.txt";
    }

    void LoadSettings()
    {
        std::ifstream stream(GetSettingsFile(), std::ios::binary);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (line.rfind("assets=", 0) == 0)     { m_SavedFolder = FromUtf8(line.substr(7)); }
            else if (line.rfind("graph=", 0) == 0) { m_SavedGraph = line.substr(6); }
            else if (line.rfind("recent=", 0) == 0) { m_Recent.push_back(line.substr(7)); }
        }
    }

    void SaveSettings() const
    {
        std::ofstream stream(GetSettingsFile(), std::ios::binary);
        if (!stream.is_open())
        {
            return; // 記憶できなくても、ツールは、使える
        }
        stream << "assets=" << ToUtf8(m_Folder) << '\n';
        stream << "graph=" << m_CurrentName << '\n';
        for (const std::string& recent : m_Recent)
        {
            stream << "recent=" << recent << '\n';
        }
    }

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

        // 最近使ったフォルダーの先頭に
        std::erase(m_Recent, ToUtf8(folder));
        m_Recent.insert(m_Recent.begin(), ToUtf8(folder));
        if (m_Recent.size() > MaxRecentFolders)
        {
            m_Recent.resize(MaxRecentFolders);
        }
        SaveSettings();
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
        m_Dirty = false;
        SaveSettings();
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
        m_Dirty = false;
        std::snprintf(m_NameText, sizeof(m_NameText), "%s", name.c_str());
        SaveSettings();
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
        m_Dirty = false;
        SaveSettings();
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
            RequestAction({ ActionKind::Folder, m_FolderText }); // Enter で開く
        }
        // 参照(フォルダーを選ぶ窓)・最近(使ったフォルダーの一覧)。文字欄は、Enter で開く
        if (ImGui::Button("参照..."))
        {
            if (const std::optional<std::filesystem::path> picked = PickFolder(m_Folder))
            {
                RequestAction({ ActionKind::Folder, ToUtf8(*picked) });
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("最近..."))
        {
            ImGui::OpenPopup("recent_popup");
        }
        if (ImGui::BeginPopup("recent_popup"))
        {
            if (m_Recent.empty())
            {
                ImGui::TextDisabled("履歴はありません");
            }
            for (size_t i = 0; i < m_Recent.size(); ++i)
            {
                const std::string label = m_Recent[i] + "##recent" + std::to_string(i);
                if (ImGui::Selectable(label.c_str()))
                {
                    RequestAction({ ActionKind::Folder, m_Recent[i] });
                }
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(文字欄は Enter)");

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
            RequestAction({ ActionKind::New, "" });
        }
        ImGui::SameLine();
        if (ImGui::Button("読み直し"))
        {
            RequestAction({ ActionKind::Reload, "" });
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
                RequestAction({ ActionKind::Open, name });
            }
        }
        ImGui::EndChild();

        DrawConfirmPopup();

        ImGui::Text("開いているグラフ:");
        ImGui::TextWrapped("%s", m_CurrentName.empty() ? "(名前のないグラフ)" : m_CurrentName.c_str());
        if (m_Dirty)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "● 保存していない変更があります");
        }
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
    bool m_Dirty = false;               // 保存していない変更がある(グラフを、編集した)
    PendingAction m_Pending;            // 確認の窓の、返事を待っている操作
    bool m_OpenConfirmPopup = false;
    std::filesystem::path m_SavedFolder; // 前回のフォルダー(設定から)
    std::string m_SavedGraph;            // 前回のグラフの名前(設定から)
    std::vector<std::string> m_Recent;   // 最近使ったフォルダー(新しい順。UTF-8)
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<EditorApp>();
}
