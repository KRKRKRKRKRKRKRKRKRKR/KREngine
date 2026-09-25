#include "TenFiftyEight/Core/Application.h"

#include "TenFiftyEight/Core/Log.h"
#include "TenFiftyEight/Renderer/Renderer.h"
#include "Platform/Windows/Window.h"

#include <chrono>

namespace TenFiftyEight
{
    Application::Application(const ApplicationDesc& desc)
    {
        m_Window = std::make_unique<Window>(WindowDesc{ desc.title, desc.width, desc.height });
        m_Window->SetResizeCallback([this](uint32_t width, uint32_t height) { OnWindowResize(width, height); });
        m_Window->SetKeyDownCallback([this](uint32_t virtualKey) { OnKeyDown(virtualKey); });
        m_Renderer = std::make_unique<Renderer>(*m_Window);
    }

    // unique_ptr<Window> の破棄には Window の完全な定義が必要なため、ヘッダーではなくここで定義する
    Application::~Application() = default;

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
                m_Renderer->EndFrame();
            }
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
#ifdef TFE_DEBUG
        // F10: シェーダーのホットリロード(Debug のみ。Release はコピーされたファイルを読むため意味がない)
        if (virtualKey == VK_F10)
        {
            m_Renderer->ReloadShaders();
        }
#else
        (void)virtualKey;
#endif
    }
}
