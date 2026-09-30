#include "Platform/Windows/Window.h"

#include "TenFiftyEight/Core/Log.h"

namespace TenFiftyEight
{
    namespace
    {
        constexpr const wchar_t* WindowClassName = L"TenFiftyEightWindow";

        std::wstring ToWide(const std::string& utf8)
        {
            const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            std::wstring wide(length, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
            return wide;
        }
    }

    Window::Window(const WindowDesc& desc)
        : m_Instance(GetModuleHandleW(nullptr))
        , m_Width(desc.width)
        , m_Height(desc.height)
    {
        // ウィンドウ作成より前に呼ぶ必要がある。モニタごとの拡大率を OS に任せず自分で扱う宣言
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &Window::WindowProc;
        windowClass.hInstance = m_Instance;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = WindowClassName;

        if (!RegisterClassExW(&windowClass))
        {
            TFE_LOG_FATAL("RegisterClassExW 失敗 (エラーコード {})", GetLastError());
        }

        // 指定サイズは「描画される部分」のサイズなので、枠とタイトルバーの分を足した全体サイズを計算する
        const DWORD style = WS_OVERLAPPEDWINDOW;
        RECT rect = { 0, 0, static_cast<LONG>(m_Width), static_cast<LONG>(m_Height) };
        AdjustWindowRectExForDpi(&rect, style, FALSE, 0, GetDpiForSystem());

        m_Handle = CreateWindowExW(
            0,
            WindowClassName,
            ToWide(desc.title).c_str(),
            style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            m_Instance,
            this); // WM_NCCREATE で受け取り、HWND と this を結びつける

        if (!m_Handle)
        {
            TFE_LOG_FATAL("CreateWindowExW 失敗 (エラーコード {})", GetLastError());
        }

        ShowWindow(m_Handle, SW_SHOW);
        TFE_LOG_INFO("ウィンドウ作成 {}x{} (DPI {})", m_Width, m_Height, GetDpiForWindow(m_Handle));
    }

    Window::~Window()
    {
        if (m_Handle)
        {
            DestroyWindow(m_Handle);
        }
        UnregisterClassW(WindowClassName, m_Instance);
    }

    bool Window::ProcessMessages()
    {
        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                return false;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return true;
    }

    LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto* window = static_cast<Window*>(createStruct->lpCreateParams);
            window->m_Handle = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }

        auto* window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (window)
        {
            return window->HandleMessage(hwnd, message, wParam, lParam);
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT Window::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        // ImGui I-4: 先に窓口に見せる(マウス・キー・文字入力を ImGui に教えるため)。処理済みならここで終わり
        if (m_MessageHook && m_MessageHook(hwnd, message, wParam, lParam))
        {
            return true;
        }

        switch (message)
        {
        case WM_SIZE:
        {
            // 最小化中は 0x0 が届く
            m_Width = LOWORD(lParam);
            m_Height = HIWORD(lParam);
            if (m_ResizeCallback)
            {
                m_ResizeCallback(m_Width, m_Height);
            }
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: // F10 と、Alt を押しながらのキーはこちらで届く
        {
            // lParam の bit30 = キーが押しっぱなしで繰り返し届いたメッセージ。最初の1回だけ反応する
            const bool isRepeat = (lParam & (1 << 30)) != 0;
            if (!isRepeat)
            {
                if (wParam == VK_F11)
                {
                    ToggleBorderless();
                    return 0;
                }
                if (m_KeyDownCallback)
                {
                    m_KeyDownCallback(static_cast<uint32_t>(wParam));
                }
            }

            // F10 は Windows では「メニューを開く」キー。既定の処理に渡すとメインループが止まるため、ここで止める
            // (Alt+F4 などは既定の処理が必要なので、F10 以外は DefWindowProc へ流す)
            if (wParam == VK_F10)
            {
                return 0;
            }
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: // WM_SYSKEYDOWN と対になる
        {
            if (m_KeyUpCallback)
            {
                m_KeyUpCallback(static_cast<uint32_t>(wParam));
            }

            // F10 は離したときもメニューを開く動きにつながるため、KEYDOWN と同じく既定の処理に渡さない
            if (wParam == VK_F10)
            {
                return 0;
            }
            break;
        }
        case WM_KILLFOCUS:
        {
            // 別のウィンドウに切り替わると、キーを離したメッセージがこちらに届かない。押しっぱなしのまま固まらないよう知らせる
            if (m_FocusLostCallback)
            {
                m_FocusLostCallback();
            }
            return 0;
        }
        case WM_DPICHANGED:
        {
            // ボーダーレス中はモニター全体を覆うサイズのままにする
            if (m_Borderless)
            {
                return 0;
            }
            // 拡大率の違うモニタへ移動したとき、OS が提案する新しい位置とサイズに合わせる
            const auto* suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_DESTROY:
            m_Handle = nullptr;
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void Window::ToggleBorderless()
    {
        // SetWindowPos の中から WM_SIZE などが届くため、先に状態を切り替えておく
        m_Borderless = !m_Borderless;

        if (m_Borderless)
        {
            // 最大化状態なども含めて、今のウィンドウの位置とサイズを覚えておく
            GetWindowPlacement(m_Handle, &m_WindowedPlacement);

            // ウィンドウがあるモニターの範囲を取得する
            MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
            GetMonitorInfoW(MonitorFromWindow(m_Handle, MONITOR_DEFAULTTONEAREST), &monitorInfo);
            const RECT& monitor = monitorInfo.rcMonitor;

            // 枠とタイトルバーを消して(WS_POPUP)、モニター全体に広げる
            SetWindowLongPtrW(m_Handle, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(m_Handle, HWND_TOP,
                monitor.left, monitor.top,
                monitor.right - monitor.left,
                monitor.bottom - monitor.top,
                SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        }
        else
        {
            // 枠を戻して、覚えておいた位置とサイズに戻す
            SetWindowLongPtrW(m_Handle, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowPlacement(m_Handle, &m_WindowedPlacement);
            SetWindowPos(m_Handle, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }

        TFE_LOG_INFO("表示モード: {}", m_Borderless ? "ボーダーレス" : "ウィンドウ");
    }
}
