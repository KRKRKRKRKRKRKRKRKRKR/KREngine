// KR エンジンのツール:アニメーションの調整ツール(KRAnimationEditor)
// ゲームのアセットのフォルダーを開いて、モデル(glTF)の骨・動き・判定の形を、見て、調整して、保存する。
// 保存した判定の形(<モデル名>.hitboxes.json)は、ゲームが AnimatedModel で読み込むだけ(ゲームに、調整の画面は、要らない)
//
//   起動:KRAnimationEditor.exe [--assets <アセットのフォルダー>] [--model <Models からの相対パス>]
//   引数なしなら、前回の続き(KRAnimationEditor.txt に、記憶している)
//   マウス:左ドラッグ = 回す / 中ドラッグ = 平行移動 / ホイール = 近づく・離れる(ImGui の窓の上では、動かさない)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <KR/KR.h>

#include "FolderDialog.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

using namespace DirectX;

namespace
{
    // path ⇔ UTF-8 の文字列(設定のファイルや ImGui の文字欄は、UTF-8)
    std::string ToUtf8(const std::filesystem::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char*>(text.data()), text.size());
    }

    std::filesystem::path FromUtf8(const std::string& text)
    {
        return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
    }

    // exe のあるフォルダー(設定のファイルの置き場所)
    std::filesystem::path GetExeDirectory()
    {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        return std::filesystem::path(exePath).parent_path();
    }

    constexpr size_t MaxRecentFolders = 8;
}

class EditorApp : public KR::Application
{
public:
    EditorApp() : Application(KR::ApplicationDesc{ .title = "KR Animation Editor" }) {}

protected:
    // 当たり判定の確認:答えが分かっている例で、Intersects を調べる。全部合っていれば OK のログ・違えばエラーのログ
    static void RunCollisionSelfTest()
    {
        int failures = 0;
        auto check = [&](const char* name, bool actual, bool expected)
        {
            if (actual != expected)
            {
                KR_LOG_ERROR("  判定の確認 NG: {}(結果 {} / 期待 {})", name, actual, expected);
                ++failures;
            }
        };

        const KR::Sphere sphere{ { 0.0f, 0.0f, 0.0f }, 1.0f };
        check("球×球 重なる", KR::Intersects(sphere, KR::Sphere{ { 1.5f, 0.0f, 0.0f }, 1.0f }), true);
        check("球×球 離れる", KR::Intersects(sphere, KR::Sphere{ { 2.5f, 0.0f, 0.0f }, 1.0f }), false);
        check("球×球 ちょうど接する", KR::Intersects(sphere, KR::Sphere{ { 2.0f, 0.0f, 0.0f }, 1.0f }), true);

        const KR::OBB box = KR::MakeOBB({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, XMMatrixIdentity());
        check("球×箱 面に近い(当たり)", KR::Intersects(KR::Sphere{ { 1.4f, 0.0f, 0.0f }, 0.5f }, box), true);
        check("球×箱 面から離れる", KR::Intersects(KR::Sphere{ { 1.6f, 0.0f, 0.0f }, 0.5f }, box), false);
        check("球×箱 角に近い(当たり)", KR::Intersects(KR::Sphere{ { 1.5f, 1.5f, 1.5f }, 0.9f }, box), true);
        check("球×箱 角から離れる", KR::Intersects(KR::Sphere{ { 1.5f, 1.5f, 1.5f }, 0.8f }, box), false);
        check("球×箱 箱の中に入っている", KR::Intersects(KR::Sphere{ { 0.2f, 0.1f, 0.0f }, 0.1f }, box), true);
        check("箱×球 引数の順を入れ替えても同じ", KR::Intersects(box, KR::Sphere{ { 1.6f, 0.0f, 0.0f }, 0.5f }), false);

        const KR::OBB rotated = KR::MakeOBB({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, XMMatrixRotationY(XM_PIDIV4));
        check("回した箱 角の先(当たり)", KR::Intersects(KR::Sphere{ { 1.7f, 0.0f, 0.0f }, 0.5f }, rotated), true);
        check("軸にそろえた箱 同じ点は外れ", KR::Intersects(KR::Sphere{ { 1.7f, 0.0f, 0.0f }, 0.5f }, box), false);

        if (failures == 0)
        {
            KR_LOG_INFO("  判定の確認: OK(球×球・球×箱・回した箱)");
        }
    }

    void OnInit() override
    {
        RunCollisionSelfTest();
        m_BoneRenderer.Create(GetRenderer());

        // 開くフォルダー・モデルの決め方:コマンドラインの引数 → 前回の記憶 → 既定(ビルドで決まった、アセットのフォルダー)
        LoadSettings();
        std::filesystem::path folder = m_LastFolder;
        std::filesystem::path model = m_LastModel;

        const std::vector<std::filesystem::path> arguments = KR::GetCommandLineArguments();
        for (size_t i = 1; i + 1 < arguments.size(); ++i)
        {
            if (arguments[i] == L"--assets")
            {
                folder = arguments[i + 1];
                model.clear(); // フォルダーを指定したら、前回のモデルは、使わない(--model で指定する)
            }
        }
        for (size_t i = 1; i + 1 < arguments.size(); ++i)
        {
            if (arguments[i] == L"--model")
            {
                model = arguments[i + 1];
            }
        }

        if (folder.empty() || !std::filesystem::is_directory(folder / "Models"))
        {
            folder = KR::GetAssetDirectory("Models").parent_path(); // 既定
        }
        ApplyFolder(folder);

        // 指定されたモデルがあれば、それを。無ければ、一覧の最初のモデルを
        if (!model.empty())
        {
            ApplyModel(model);
        }
        else if (!m_Models.empty())
        {
            ApplyModel(FromUtf8(m_Models.front()));
        }
    }

    void OnUpdate(float deltaTime) override
    {
        if (m_Rotate)
        {
            m_Angle = std::fmod(m_Angle + deltaTime * 0.5f, 2.0f * std::numbers::pi_v<float>);
        }
        UpdateCamera();
        m_Model.Update(deltaTime); // 時刻を進めて、姿勢を求める
    }

    void OnRender() override
    {
        if (!m_Model.IsLoaded())
        {
            return;
        }

        // 形の中心を原点に動かしてから、Y 軸で回す
        const XMMATRIX world = XMMatrixTranslation(-m_Center.x, -m_Center.y, -m_Center.z) * XMMatrixRotationY(m_Angle);

        if (m_ShowMesh)
        {
            m_Model.Draw(GetRenderer(), world);
        }
        if (m_ShowBones)
        {
            KR::BoneRenderer::Style style;
            style.selectedJoint = m_SelectedBone; // 選んだ骨は、色が変わる
            style.inFront = m_BonesInFront;
            m_BoneRenderer.Draw(GetRenderer(), m_Model.GetSkeleton(), m_Model.GetPose(), world, style);
        }

        if (m_ShowHitboxes)
        {
            // テストの箱(世界に固定。ImGui で動かす)
            m_TestBox.center = m_TestCenter;
            GetRenderer().DrawWireBox(m_TestBox, { 0.3f, 0.8f, 1.0f, 1.0f });
            KR::WorldHitbox testShape;
            testShape.shape = KR::HitShape::Box;
            testShape.box = m_TestBox;

            // 判定の形を、今の姿勢で、世界の座標にして、描く
            const std::vector<KR::WorldHitbox> shapes = m_Model.GetWorldHitboxes(world);
            m_HitIndices.clear();
            for (size_t i = 0; i < shapes.size(); ++i)
            {
                const bool hit = KR::Intersects(shapes[i], testShape);
                if (hit)
                {
                    m_HitIndices.push_back(i);
                }
                // 当たり = 赤 / 選んでいる形 = 黄 / ほか = 緑
                const bool isSelected = (i == m_SelectedHitbox);
                const XMFLOAT4 color = hit ? XMFLOAT4{ 1.0f, 0.2f, 0.2f, 1.0f }
                                     : isSelected ? XMFLOAT4{ 1.0f, 0.9f, 0.1f, 1.0f } : XMFLOAT4{ 0.2f, 1.0f, 0.2f, 1.0f };
                if (shapes[i].shape == KR::HitShape::Box)
                {
                    GetRenderer().DrawWireBox(shapes[i].box, color);
                }
                else
                {
                    GetRenderer().DrawWireSphere(shapes[i].sphere, color);
                }
            }
        }
    }

    void OnImGui() override
    {
        ImGui::Begin("Animation Editor");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        DrawProjectSection();

        if (m_Model.IsLoaded())
        {
            DrawViewSection();
            DrawPlaybackSection();
            DrawHitboxSection();
        }
        else
        {
            ImGui::Separator();
            ImGui::TextWrapped(m_Models.empty() ? "No .glb found in <Assets>/Models. Choose another folder." : "Model not loaded (see log).");
        }
        ImGui::End();

        DrawUnsavedPopup();
    }

    void OnShutdown() override
    {
        m_BoneRenderer.Destroy(GetRenderer());
        m_Model.Unload(GetRenderer());
    }

private:
    // ---- 開く(フォルダー・モデル)----

    // アセットのフォルダーを決める:KR::SetAssetRoot で、エンジンの読み書きの場所を切り替え、Models/ の下の .glb を、一覧にする
    // Models/ が無いフォルダーは、断る(false)
    bool ApplyFolder(const std::filesystem::path& folder)
    {
        std::error_code error;
        if (!std::filesystem::is_directory(folder / "Models", error))
        {
            KR_LOG_WARN("Editor: {} に、Models フォルダーがありません", ToUtf8(folder));
            m_StatusMessage = "No Models folder in: " + ToUtf8(folder);
            return false;
        }

        KR::SetAssetRoot(folder);
        m_Folder = folder;
        std::snprintf(m_FolderText, sizeof(m_FolderText), "%s", ToUtf8(folder).c_str());

        // Models/ の下の .glb / .gltf を、全部探す(Models からの相対パス。区切りは /)
        m_Models.clear();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folder / "Models", std::filesystem::directory_options::skip_permission_denied, error))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            std::filesystem::path extension = entry.path().extension();
            std::wstring lower = extension.wstring();
            std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            if (lower == L".glb" || lower == L".gltf")
            {
                m_Models.push_back(ToUtf8(std::filesystem::relative(entry.path(), folder / "Models").generic_u8string()));
            }
        }
        std::sort(m_Models.begin(), m_Models.end());
        m_SelectedModel = 0;

        // 最近使ったフォルダーの先頭に
        std::erase(m_Recent, ToUtf8(folder));
        m_Recent.insert(m_Recent.begin(), ToUtf8(folder));
        if (m_Recent.size() > MaxRecentFolders)
        {
            m_Recent.resize(MaxRecentFolders);
        }

        m_StatusMessage = m_Models.empty() ? "No .glb found in <Assets>/Models" : "";
        KR_LOG_INFO("Editor: アセットのフォルダー {}(モデル {} 個)", ToUtf8(folder), m_Models.size());
        return true;
    }

    // モデルを読む(Models からの相対パス)。読めたら、カメラ・選択・「未保存」を、初期化する
    bool ApplyModel(const std::filesystem::path& relative)
    {
        m_Dirty = false;
        m_CurrentModel.clear();
        if (!m_Model.Load(GetRenderer(), relative))
        {
            m_StatusMessage = "Failed to load: " + ToUtf8(relative);
            return false;
        }
        m_CurrentModel = ToUtf8(relative.generic_u8string());
        m_StatusMessage = "";

        // 一覧の中で、今のモデルを選んでおく
        for (size_t i = 0; i < m_Models.size(); ++i)
        {
            if (m_Models[i] == m_CurrentModel)
            {
                m_SelectedModel = i;
            }
        }

        const KR::AnimatedModel::Bounds& bounds = m_Model.GetBounds();
        m_Center = bounds.Center();
        m_ModelRadius = bounds.Radius();
        m_TestBox.halfSize = { m_ModelRadius * 0.15f, m_ModelRadius * 0.15f, m_ModelRadius * 0.15f };
        m_TestCenter = { m_ModelRadius * 0.8f, 0.0f, 0.0f }; // 最初は、モデルの横に置く(スライダーで、動かして、骨の形に、ぶつける)
        m_SelectedBone = 0;
        m_SubHitbox = 0;
        m_SelectedHitbox = NoHitbox;
        m_BindPose = false;
        m_LastClip = 0;
        ResetCamera();
        SaveSettings();
        return true;
    }

    // 未保存の変更があれば、確認の窓を出してから(DrawUnsavedPopup)。無ければ、すぐ
    void RequestFolder(const std::filesystem::path& folder)
    {
        if (m_Dirty)
        {
            m_Pending = Pending::Folder;
            m_PendingPath = folder;
            return;
        }
        if (ApplyFolder(folder))
        {
            OpenFirstModelOrUnload();
        }
    }

    void RequestModel(const std::filesystem::path& relative)
    {
        if (m_Dirty)
        {
            m_Pending = Pending::Model;
            m_PendingPath = relative;
            return;
        }
        ApplyModel(relative);
    }

    // フォルダーを替えたあと:新しいフォルダーの、最初のモデルを開く(無ければ、今のモデルを閉じる)
    void OpenFirstModelOrUnload()
    {
        if (!m_Models.empty())
        {
            ApplyModel(FromUtf8(m_Models.front()));
        }
        else
        {
            m_Model.Unload(GetRenderer());
            m_CurrentModel.clear();
            m_Dirty = false;
        }
        SaveSettings();
    }

    void ApplyPending()
    {
        const Pending kind = m_Pending;
        const std::filesystem::path path = m_PendingPath;
        m_Pending = Pending::None;
        if (kind == Pending::Folder)
        {
            if (ApplyFolder(path))
            {
                OpenFirstModelOrUnload();
            }
        }
        else if (kind == Pending::Model)
        {
            ApplyModel(path);
        }
    }

    // ---- 設定の記憶(exe の隣の KRAnimationEditor.txt。1 行 1 項目。UTF-8)----

    void LoadSettings()
    {
        std::ifstream stream(GetExeDirectory() / "KRAnimationEditor.txt", std::ios::binary);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (line.rfind("assets=", 0) == 0)      { m_LastFolder = FromUtf8(line.substr(7)); }
            else if (line.rfind("model=", 0) == 0)  { m_LastModel = FromUtf8(line.substr(6)); }
            else if (line.rfind("recent=", 0) == 0) { m_Recent.push_back(line.substr(7)); }
        }
    }

    void SaveSettings() const
    {
        std::ofstream stream(GetExeDirectory() / "KRAnimationEditor.txt", std::ios::binary);
        if (!stream.is_open())
        {
            return; // 記憶できなくても、ツールは、使える
        }
        stream << "assets=" << ToUtf8(m_Folder) << '\n';
        stream << "model=" << m_CurrentModel << '\n';
        for (const std::string& recent : m_Recent)
        {
            stream << "recent=" << recent << '\n';
        }
    }

    // ---- カメラ(マウス)----

    void ResetCamera()
    {
        m_Yaw = 0.0f;
        m_Pitch = 0.15f;
        m_Distance = std::max(m_ModelRadius, 0.1f) * 3.0f;
        m_Target = { 0.0f, 0.0f, 0.0f };
    }

    void UpdateCamera()
    {
        // ImGui の窓の上・ImGui を操作している間は、カメラを動かさない
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse)
        {
            if (io.MouseDown[0]) // 左ドラッグ:回す
            {
                m_Yaw -= io.MouseDelta.x * 0.01f;
                m_Pitch = std::clamp(m_Pitch + io.MouseDelta.y * 0.01f, -1.5f, 1.5f);
            }
            if (io.MouseDown[2]) // 中ドラッグ:平行移動(カメラの右・上の向きに、距離に比例した量)
            {
                const float scale = m_Distance * 0.002f;
                m_Target.x -= std::cos(m_Yaw) * io.MouseDelta.x * scale;
                m_Target.z -= std::sin(m_Yaw) * io.MouseDelta.x * scale;
                m_Target.y += io.MouseDelta.y * scale;
            }
            if (io.MouseWheel != 0.0f) // ホイール:近づく・離れる
            {
                m_Distance = std::clamp(m_Distance * std::pow(0.9f, io.MouseWheel), m_ModelRadius * 0.2f + 0.01f, m_ModelRadius * 20.0f + 1.0f);
            }
        }

        // 注目点のまわりを、回る位置にカメラを置く(yaw = 0 で、手前(−Z)から見る)
        KR::Camera camera;
        camera.SetPosition({ m_Target.x + m_Distance * std::sin(m_Yaw) * std::cos(m_Pitch),
                             m_Target.y + m_Distance * std::sin(m_Pitch),
                             m_Target.z - m_Distance * std::cos(m_Yaw) * std::cos(m_Pitch) });
        camera.LookAt(m_Target);
        GetRenderer().SetCamera(camera);
    }

    // ---- ImGui の各区画 ----

    void DrawProjectSection()
    {
        ImGui::SeparatorText("Project");

        // アセットのフォルダー:文字欄(Enter で開く)・Browse・最近使った一覧
        ImGui::SetNextItemWidth(-130.0f);
        const bool entered = ImGui::InputText("##folder", m_FolderText, sizeof(m_FolderText), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Browse...") )
        {
            if (const std::optional<std::filesystem::path> picked = PickFolder(m_Folder))
            {
                RequestFolder(*picked);
            }
        }
        ImGui::SameLine();
        ImGui::Text("Assets");
        if (entered)
        {
            RequestFolder(FromUtf8(m_FolderText));
        }

        if (!m_Recent.empty() && ImGui::BeginCombo("Recent", "Choose..."))
        {
            for (size_t i = 0; i < m_Recent.size(); ++i)
            {
                const std::string label = m_Recent[i] + "##recent" + std::to_string(i);
                if (ImGui::Selectable(label.c_str()))
                {
                    RequestFolder(FromUtf8(m_Recent[i]));
                }
            }
            ImGui::EndCombo();
        }

        // モデルの一覧(Models の下の .glb)
        if (!m_Models.empty())
        {
            m_SelectedModel = std::min(m_SelectedModel, m_Models.size() - 1);
            const std::string preview = m_CurrentModel.empty() ? "Choose..." : m_CurrentModel;
            if (ImGui::BeginCombo("Model", preview.c_str()))
            {
                for (size_t i = 0; i < m_Models.size(); ++i)
                {
                    const std::string label = m_Models[i] + "##model" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), m_Models[i] == m_CurrentModel))
                    {
                        RequestModel(FromUtf8(m_Models[i]));
                    }
                }
                ImGui::EndCombo();
            }
        }
        if (!m_StatusMessage.empty())
        {
            ImGui::TextColored({ 1.0f, 0.5f, 0.3f, 1.0f }, "%s", m_StatusMessage.c_str());
        }
        if (m_Dirty)
        {
            ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "* Unsaved hitbox changes");
        }
    }

    void DrawViewSection()
    {
        ImGui::SeparatorText("View");
        ImGui::Text("Joints: %zu / Clips: %zu", m_Model.GetSkeleton().joints.size(), m_Model.GetClipCount());
        ImGui::Checkbox("Bones", &m_ShowBones);
        ImGui::Checkbox("Bones in front", &m_BonesInFront); // 骨を、メッシュの手前に描く(メッシュの中の骨も見える)
        ImGui::Checkbox("Mesh", &m_ShowMesh);
        ImGui::Checkbox("Rotate model", &m_Rotate);
        if (ImGui::Button("Reset camera"))
        {
            ResetCamera();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(drag: orbit / middle: pan / wheel: zoom)");

        // 選んだ骨:一覧(コンボボックス)から、名前で選ぶ。選んだ骨は橙色になり、その骨の判定の形(下)が、調整できる
        // 一覧は、骨の階層に合わせて、字下げする(子の骨ほど、右へ)
        const KR::Skeleton& skeleton = m_Model.GetSkeleton();
        const int jointCount = static_cast<int>(skeleton.joints.size());
        m_SelectedBone = std::clamp(m_SelectedBone, 0, std::max(jointCount - 1, 0));
        if (ImGui::BeginCombo("Bone", skeleton.joints[static_cast<size_t>(m_SelectedBone)].name.c_str()))
        {
            for (int i = 0; i < jointCount; ++i)
            {
                int depth = 0; // 根から、何段目か(親をたどって数える)
                for (int p = skeleton.joints[static_cast<size_t>(i)].parent; p >= 0 && depth < jointCount; p = skeleton.joints[static_cast<size_t>(p)].parent)
                {
                    ++depth;
                }
                const std::string label = std::string(static_cast<size_t>(depth) * 2, ' ') + skeleton.joints[static_cast<size_t>(i)].name + "##bone" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), i == m_SelectedBone))
                {
                    m_SelectedBone = i;
                    m_SubHitbox = 0;
                }
            }
            ImGui::EndCombo();
        }
    }

    void DrawPlaybackSection()
    {
        ImGui::SeparatorText("Playback");
        if (m_Model.GetClipCount() == 0)
        {
            ImGui::Text("No animation clips");
            return;
        }

        // 「Bind pose」がオンの間は、動きを止めて、基準姿勢にしている。オフにすると、選んでいる動きを、最初から再生する
        if (m_Model.GetCurrentClip() >= 0)
        {
            m_LastClip = static_cast<size_t>(m_Model.GetCurrentClip());
        }
        m_LastClip = std::min(m_LastClip, m_Model.GetClipCount() - 1);

        // 動きの一覧(名前で選ぶ)
        if (ImGui::BeginCombo("Clip", m_Model.GetClipName(m_LastClip).c_str()))
        {
            for (size_t i = 0; i < m_Model.GetClipCount(); ++i)
            {
                const std::string label = m_Model.GetClipName(i) + "##clip" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), i == m_LastClip))
                {
                    m_LastClip = i;
                    if (!m_BindPose)
                    {
                        m_Model.Play(m_LastClip, m_Model.IsLooping());
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Text("Length: %.3f s", m_Model.GetDuration());

        bool paused = m_Model.IsPaused();
        if (ImGui::Checkbox("Pause", &paused))
        {
            m_Model.Pause(paused);
        }
        bool loop = m_Model.IsLooping();
        if (ImGui::Checkbox("Loop", &loop))
        {
            m_Model.SetLooping(loop);
            if (loop && !m_BindPose)
            {
                m_Model.Play(m_LastClip, true); // ループに戻したら、最初から
            }
        }
        ImGui::SameLine();
        ImGui::Text(m_Model.IsFinished() ? "(finished)" : "");
        float speed = m_Model.GetSpeed();
        if (ImGui::SliderFloat("Speed", &speed, 0.0f, 3.0f))
        {
            m_Model.SetSpeed(speed);
        }
        float time = m_Model.GetTime();
        if (ImGui::SliderFloat("Time", &time, 0.0f, m_Model.GetDuration())) // 止めているときに、時刻を動かして、姿勢を確かめる
        {
            m_Model.SetTime(time);
        }

        // 基準姿勢(動かない姿勢)のオン・オフ。オンで、動きを止める。オフで、選んでいる動きを、最初から再生する
        if (ImGui::Checkbox("Bind pose (no animation)", &m_BindPose))
        {
            if (m_BindPose)
            {
                m_Model.Stop();
            }
            else
            {
                m_Model.Play(m_LastClip, m_Model.IsLooping());
            }
        }
    }

    void DrawHitboxSection()
    {
        ImGui::SeparatorText("Hitboxes");
        const KR::Skeleton& skeleton = m_Model.GetSkeleton();
        const int jointCount = static_cast<int>(skeleton.joints.size());

        ImGui::Checkbox("Show hitboxes", &m_ShowHitboxes);
        const float range = m_ModelRadius * 1.5f;
        ImGui::SliderFloat3("Test box position", &m_TestCenter.x, -range, range);
        std::vector<KR::Hitbox>& hitboxes = m_Model.GetHitboxes();
        ImGui::Text("Hit: %zu / %zu", m_HitIndices.size(), hitboxes.size());
        for (const size_t index : m_HitIndices)
        {
            // 触れている形が、どの骨の物か(名前で表示)
            const int joint = hitboxes[index].jointIndex;
            ImGui::BulletText("%s", joint >= 0 && joint < jointCount ? skeleton.joints[static_cast<size_t>(joint)].name.c_str() : "(no joint)");
        }

        // 選んだ骨(View の Bone)にくっついた、判定の形を調整する。形は、骨の中の座標で持つ(骨が動いても、この数字は変わらない)
        // 1 本の骨に、複数の形も付けられる(その場合だけ、「Hitbox on this bone」で選ぶ)
        std::vector<size_t> onBone;
        for (size_t i = 0; i < hitboxes.size(); ++i)
        {
            if (hitboxes[i].jointIndex == m_SelectedBone)
            {
                onBone.push_back(i);
            }
        }

        m_SelectedHitbox = NoHitbox;
        if (onBone.empty())
        {
            ImGui::Text("No hitbox on this bone");
        }
        else
        {
            m_SubHitbox = std::clamp(m_SubHitbox, 0, static_cast<int>(onBone.size()) - 1);
            if (onBone.size() > 1)
            {
                ImGui::SliderInt("Hitbox on this bone", &m_SubHitbox, 0, static_cast<int>(onBone.size()) - 1);
            }
            m_SelectedHitbox = onBone[static_cast<size_t>(m_SubHitbox)];
            KR::Hitbox& hitbox = hitboxes[m_SelectedHitbox];

            // 変えたら「未保存」の印(m_Dirty)
            int shape = (hitbox.shape == KR::HitShape::Box) ? 0 : 1;
            if (ImGui::Combo("Shape", &shape, "Box\0Sphere\0"))
            {
                m_Dirty = true;
            }
            hitbox.shape = (shape == 0) ? KR::HitShape::Box : KR::HitShape::Sphere;
            m_Dirty |= ImGui::DragFloat3("Center (in joint)", &hitbox.center.x, 0.05f);
            if (hitbox.shape == KR::HitShape::Box)
            {
                m_Dirty |= ImGui::DragFloat3("Half size", &hitbox.halfSize.x, 0.05f, 0.01f, 100.0f);
            }
            else
            {
                m_Dirty |= ImGui::DragFloat("Radius", &hitbox.radius, 0.05f, 0.01f, 100.0f);
            }
        }

        // 形の追加・削除(追加・削除のあとは、形の一覧が変わるので、最後にまとめて行う)
        if (ImGui::Button("Add hitbox to this bone"))
        {
            KR::Hitbox added;
            added.name = skeleton.joints[static_cast<size_t>(m_SelectedBone)].name;
            added.jointIndex = m_SelectedBone;
            const float size = m_ModelRadius * 0.1f;
            added.halfSize = { size, size, size };
            added.radius = size;
            hitboxes.push_back(added);
            m_SubHitbox = static_cast<int>(onBone.size()); // 追加した形を、選ぶ
            m_Dirty = true;
        }
        if (m_SelectedHitbox != NoHitbox)
        {
            ImGui::SameLine();
            if (ImGui::Button("Remove hitbox"))
            {
                hitboxes.erase(hitboxes.begin() + static_cast<std::ptrdiff_t>(m_SelectedHitbox));
                m_SelectedHitbox = NoHitbox;
                m_SubHitbox = 0;
                m_Dirty = true;
            }
        }

        // 保存・読み直し(<モデル名>.hitboxes.json。開いているアセットのフォルダーの、Models の下に書く)
        ImGui::Separator();
        if (ImGui::Button(m_Dirty ? "Save hitboxes *" : "Save hitboxes"))
        {
            if (m_Model.SaveHitboxes())
            {
                m_Dirty = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload hitboxes"))
        {
            m_Model.ReloadHitboxes();
            m_Dirty = false;
        }
    }

    // 未保存のまま、別のモデル・フォルダーに切り替えようとしたときの、確認の窓
    void DrawUnsavedPopup()
    {
        if (m_Pending != Pending::None && !ImGui::IsPopupOpen("Unsaved changes"))
        {
            ImGui::OpenPopup("Unsaved changes");
        }
        if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("The hitboxes of the current model have unsaved changes.");
            if (ImGui::Button("Save and continue"))
            {
                if (m_Model.SaveHitboxes())
                {
                    m_Dirty = false;
                    ApplyPending();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard and continue"))
            {
                m_Dirty = false;
                ApplyPending();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_Pending = Pending::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

private:
    KR::AnimatedModel m_Model;       // 開いているモデル(形・骨・動き・判定の形)
    KR::BoneRenderer m_BoneRenderer; // 骨を、八面体の形(Blender の骨に似せた形)で描く(デバッグ用)

    // 開いているもの
    std::filesystem::path m_Folder;        // アセットのフォルダー(KR::SetAssetRoot に渡した物)
    std::vector<std::string> m_Models;     // Models の下の .glb(相対パス。UTF-8。区切りは /)
    size_t m_SelectedModel = 0;
    std::string m_CurrentModel;            // 今読んでいるモデル(Models からの相対パス)
    char m_FolderText[1024] = {};          // フォルダーの文字欄
    std::vector<std::string> m_Recent;     // 最近使ったフォルダー(新しい順)
    std::filesystem::path m_LastFolder;    // 前回のフォルダー・モデル(記憶から読んだ物。起動のときだけ使う)
    std::filesystem::path m_LastModel;
    std::string m_StatusMessage;           // 画面に出す、エラーなどの文字

    // 未保存の確認
    bool m_Dirty = false;                  // 判定の形を変えて、まだ保存していない
    enum class Pending { None, Folder, Model };
    Pending m_Pending = Pending::None;     // 未保存のとき、確認のあとに行う、切り替え
    std::filesystem::path m_PendingPath;

    // カメラ(マウスで動かす)
    float m_Yaw = 0.0f;
    float m_Pitch = 0.15f;
    float m_Distance = 10.0f;
    XMFLOAT3 m_Target = {};

    XMFLOAT3 m_Center = {};          // 形の中心
    float m_ModelRadius = 1.0f;      // モデルの大きさ(スライダーの範囲に使う)
    float m_Angle = 0.0f;
    int m_SelectedBone = 0;          // 選んだ骨(関節の番号)。色が変わる。判定の形の調整は、この骨の形について行う
    bool m_ShowBones = true;
    bool m_BonesInFront = true;
    bool m_ShowMesh = true;
    bool m_Rotate = false;

    // 判定の形
    static constexpr size_t NoHitbox = static_cast<size_t>(-1); // 調整する形が無い(選んだ骨に、形が無い)
    size_t m_SelectedHitbox = NoHitbox;    // 調整している形(選んだ骨の形。毎フレーム、ImGui が決める)
    int m_SubHitbox = 0;                   // 選んだ骨に、形が複数あるときの、何番目か
    bool m_BindPose = false;               // 動きを止めて、基準姿勢にしているか
    size_t m_LastClip = 0;                 // 基準姿勢をオフにしたときに、再生する動き
    bool m_ShowHitboxes = true;
    XMFLOAT3 m_TestCenter = {};            // テストの箱の位置(世界)
    KR::OBB m_TestBox;                     // テストの箱(軸にそろえた箱)
    std::vector<size_t> m_HitIndices;      // 今、テストの箱に触れている形の番号
};

std::unique_ptr<KR::Application> KR::CreateApplication()
{
    return std::make_unique<EditorApp>();
}
