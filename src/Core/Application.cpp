#include "TenFiftyEight/Core/Application.h"

#include "TenFiftyEight/Core/Log.h"
#include "TenFiftyEight/Renderer/Renderer.h"
#include "Core/InputState.h"
#include "Platform/Windows/Window.h"

#include <chrono>

namespace TenFiftyEight
{
    Application::Application(const ApplicationDesc& desc)
    {
        m_Window = std::make_unique<Window>(WindowDesc{ desc.title, desc.width, desc.height });
        m_Window->SetResizeCallback([this](uint32_t width, uint32_t height) { OnWindowResize(width, height); });
        m_Window->SetKeyDownCallback([this](uint32_t virtualKey) { OnKeyDown(virtualKey); });
        // 3-7-2: キーを離した・非アクティブになった、を入力の状態表へ書く(Window は InputState を知らない)
        m_Window->SetKeyUpCallback([](uint32_t virtualKey) { InputState::OnKeyUp(virtualKey); });
        m_Window->SetFocusLostCallback([]() { InputState::ReleaseAll(); });
        m_Renderer = std::make_unique<Renderer>(*m_Window);

        // ImGui I-4: メッセージを、Window が処理する前に ImGui に見せる(Window は ImGui を知らない。つなぐのは Application)
        m_Window->SetMessageHook([this](HWND window, UINT message, WPARAM wParam, LPARAM lParam)
        {
            return m_Renderer && m_Renderer->HandleWindowMessage(window, message, wParam, lParam);
        });
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

            OnUpdate(deltaTime);

            if (!m_Minimized)
            {
                m_Renderer->BeginFrame();
                OnRender();
                OnImGui();
                m_Renderer->EndFrame();
            }

            // 「このフレームで押された・離された」の付箋を剥がす。OnUpdate も OnRender も読み終わった後
            // 最小化中(描画しないとき)も剥がし忘れないよう、描画の有無に関係なく毎回呼ぶ
            InputState::EndFrame();
        }

        OnShutdown();
    }

    void Application::OnWindowResize(uint32_t width, uint32_t height)
    {
        m_Minimized = (width == 0 || height == 0);
        TFE_LOG_TRACE("ウィンドウサイズ変更 {}x{}", width, height);

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

#ifdef TFE_DEBUG
        // F10: シェーダーのホットリロード(Debug のみ。Release はコピーされたファイルを読むため意味がない)
        if (virtualKey == VK_F10)
        {
            m_Renderer->ReloadShaders();
        }
#endif
    }
}
