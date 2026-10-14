#include "KR/Core/Application.h"

#include "KR/Core/Log.h"
#include "KR/Renderer/Renderer.h"
#include "KR/Audio/Audio.h"
#include "KR/Core/Assets.h"
#include "KR/Behavior/LiveGraph.h"
#include "Behavior/ObserveState.h"
#include "Core/EditorLauncher.h"
#include "Core/InputState.h"
#include "Platform/Windows/Window.h"

#include <imgui.h>

#include <chrono>

namespace KR
{
    Application::Application(const ApplicationDesc& desc)
    {
        m_Window = std::make_unique<Window>(WindowDesc{ desc.title, desc.width, desc.height });
        m_Window->SetResizeCallback([this](uint32_t width, uint32_t height) { OnWindowResize(width, height); });
        m_Window->SetKeyDownCallback([this](uint32_t virtualKey) { OnKeyDown(virtualKey); });
        // 3-7-2: キーを離した・非アクティブになった、を入力の状態表へ書く(Window は InputState を知らない)
        m_Window->SetKeyUpCallback([](uint32_t virtualKey) { InputState::OnKeyUp(virtualKey); });
        m_Window->SetFocusLostCallback([]() { InputState::ReleaseAll(); });

        // マウス:ボタンは、キーと同じ状態表に書く(VK_LBUTTON などの番号)
        // ImGui の窓の上では、「押した」とホイールをゲームに渡さない。「離した」と位置は、いつも書く(押しっぱなしで固まらないように)
        m_Window->SetMouseButtonDownCallback([this](uint32_t virtualKey)
        {
            if (!(m_Renderer && m_Renderer->ImGuiWantsMouse()))
            {
                InputState::OnKeyDown(virtualKey);
            }
        });
        m_Window->SetMouseButtonUpCallback([](uint32_t virtualKey) { InputState::OnKeyUp(virtualKey); });
        m_Window->SetMouseMoveCallback([](int32_t x, int32_t y) { InputState::OnMouseMove(x, y); });
        m_Window->SetMouseWheelCallback([this](float notches)
        {
            if (!(m_Renderer && m_Renderer->ImGuiWantsMouse()))
            {
                InputState::OnMouseWheel(notches);
            }
        });
        m_Renderer = std::make_unique<Renderer>(*m_Window);

        // ImGui I-4: メッセージを、Window が処理する前に ImGui に見せる(Window は ImGui を知らない。つなぐのは Application)
        m_Window->SetMessageHook([this](HWND window, UINT message, WPARAM wParam, LPARAM lParam)
        {
            return m_Renderer && m_Renderer->HandleWindowMessage(window, message, wParam, lParam);
        });

        // 音。音の機器が無くても失敗しない(警告を出して、音を出さずに動く)
        m_Audio = std::make_unique<Audio>();

#ifdef KR_DEBUG
        // F1 でツールを開く係(Debug だけ。Release のゲームには、編集の機能を入れない)
        if (desc.enableEditorKey)
        {
            m_EditorLauncher = std::make_unique<EditorLauncher>();
        }
#endif
        m_PauseDefault = desc.pauseGameWhileEditing;
        m_PauseWhileEditing = m_PauseDefault;
    }

    // unique_ptr<Window> の破棄には Window の完全な定義が必要なため、ヘッダーではなくここで定義する
    Application::~Application()
    {
        // Renderer が先に破棄されるので、その後にウィンドウが受けるメッセージを、Renderer に渡さないようにする
        if (m_Window)
        {
            m_Window->SetMessageHook(nullptr);
        }
    }

    //アプリケーションの更新処理
    void Application::Run()
    {
        // コンストラクタ内では派生クラスの仮想関数が呼ばれないため、OnInit はここで呼ぶ
        OnInit();

        using Clock = std::chrono::steady_clock;
        Clock::time_point previousTime = Clock::now();

        while (m_Window->ProcessMessages())
        {
            const Clock::time_point currentTime = Clock::now();
            const float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
            previousTime = currentTime;

            // ツールが開いていて、「ゲームを止める」なら、OnUpdate を呼ばない(動き・タイマー・敵の行動が、全部止まる)
            // 描画・ImGui・音は続ける(止まった画面を見ながら、窓を操作できる)
            const bool editorOpen = m_EditorLauncher && m_EditorLauncher->IsRunning();
            if (m_EditorWasOpen && !editorOpen)
            {
                // ツールが閉じた(落ちた場合も):保存していない変更のライブファイルを消す → ゲームは、保存の内容に戻る
                Behavior::DeleteAllLiveGraphFiles(GetAssetDirectory("Enemies"));
            }
            m_EditorWasOpen = editorOpen;
            const bool paused = editorOpen && m_PauseWhileEditing;
            if (!paused)
            {
                OnUpdate(deltaTime);
            }

            // 見ている敵の今の状態(ゲームが OnUpdate で ObserveRunner した物)を、約 0.1 秒おきに観測ファイルへ(Debug だけ。F1 のツールが光らせる)
            Behavior::ObserveState::EndFrame(deltaTime, paused, GetAssetDirectory("Enemies"));

            // 鳴り終わった音(声)を片付ける。OnUpdate で鳴らした・止めた後
            m_Audio->Update();

            if (!m_Minimized)
            {
                m_Renderer->BeginFrame();
                OnRender();
                OnImGui();
                if (editorOpen)
                {
                    DrawEditorPanel(); // ゲームの窓より後 = 手前に出る
                }
                m_Renderer->EndFrame();
            }

            // 「このフレームで押された・離された」の付箋を剥がす。OnUpdate も OnRender も読み終わった後
            // 最小化中(描画しないとき)も剥がし忘れないよう、描画の有無に関係なく毎回呼ぶ
            InputState::EndFrame();
        }

        OnShutdown();
        Behavior::ObserveState::Shutdown(GetAssetDirectory("Enemies")); // ツールは、すぐ「ゲームとつながっていません」になる
    }

    void Application::OnWindowResize(uint32_t width, uint32_t height)
    {
        m_Minimized = (width == 0 || height == 0);
        KR_LOG_TRACE("ウィンドウサイズ変更 {}x{}", width, height);

        // 最小化(0x0)のときはバックバッファを作れないので、元のサイズのまま保持する
        if (!m_Minimized && m_Renderer)
        {
            m_Renderer->Resize(width, height);
        }
    }

    void Application::OnKeyDown(uint32_t virtualKey)
    {
        // ImGui I-6a: ImGui が文字入力などでキーボードを使っている間は、ゲームにキーを渡さない(F10 なども含む)
        // 「離した」(OnKeyUp)は、押しっぱなしのまま固まらないよう、いつも状態表に伝える(コンストラクタの登録を参照)
        if (m_Renderer && m_Renderer->ImGuiWantsKeyboard())
        {
            return;
        }

        // 3-7-2: 入力の状態表へ「押された」を書く(押しっぱなしの繰り返しは Window 側で除かれている)
        InputState::OnKeyDown(virtualKey);

#ifdef KR_DEBUG
        // F10: シェーダーのホットリロード(Debug のみ。Release はコピーされたファイルを読むため意味がない)
        if (virtualKey == VK_F10)
        {
            m_Renderer->ReloadShaders();
        }

        // F1: 敵の行動のツールを開く(Debug のみ。ツールで保存すると、ゲームが読み直す)
        if (virtualKey == VK_F1 && m_EditorLauncher)
        {
            OpenBehaviorEditor();
        }
#endif
    }

    void Application::OpenBehaviorEditor()
    {
        // ツールは、ゲームの exe の横(Debug のビルドのとき、kr_copy_runtime_files がコピーする)
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        const std::filesystem::path tool = std::filesystem::path(exePath).parent_path() / L"KRBehaviorEditor.exe";

        // 渡すのは、ゲームの Assets フォルダー(Debug = 元のフォルダー。ツールの保存が、そのままゲームに届く)
        const std::filesystem::path assets = GetAssetDirectory("Enemies").parent_path().make_preferred();

        const bool wasRunning = m_EditorLauncher->IsRunning();
        if (m_EditorLauncher->OpenOrFocus(tool, { L"--assets", assets.wstring() }) && !wasRunning)
        {
            m_PauseWhileEditing = m_PauseDefault; // 開くたびに、初期値(ApplicationDesc::pauseGameWhileEditing)に戻す。小さな窓で切り替えられる
        }
    }

    void Application::DrawEditorPanel()
    {
        // 右上に、小さく出す(ゲームの窓と重なりにくいように)
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 10.0f, viewport->WorkPos.y + 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("エディター(F1)", nullptr, flags))
        {
            ImGui::TextUnformatted("敵の行動のツールを開いています");
            ImGui::Checkbox("ゲームを止める", &m_PauseWhileEditing);
            ImGui::TextDisabled("ツールで保存すると、ゲームが読み直します");
        }
        ImGui::End();
    }
}
