#include "Platform/Windows/Window.h"

#include "KR/Core/Log.h"
#include "Platform/Windows/KeyCodes.h"

#include <windowsx.h> // GET_X_LPARAM・GET_Y_LPARAM(マウスの位置。負の値も正しく取り出す)

namespace KR
{
    namespace
    {
        constexpr const wchar_t* WindowClassName = L"KRWindow";

        std::wstring ToWide(const std::string& utf8)
        {
            const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            std::wstring wide(length, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
            return wide;
        }

        // キーのメッセージの wParam を、入力の状態表のキー番号に直す
        // テンキーの Enter は、上の Enter と同じ VK_RETURN で届く。lParam の bit24(拡張キーの印)が立っていれば、テンキーの Enter
        uint32_t ToKeyNumber(WPARAM wParam, LPARAM lParam)
        {
            const bool isExtended = (lParam & (1 << 24)) != 0;
            if (wParam == VK_RETURN && isExtended)
            {
                return KeyCodes::NumpadEnter;
            }
            return static_cast<uint32_t>(wParam);
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
            KR_LOG_FATAL("RegisterClassExW 失敗 (エラーコード {})", GetLastError());
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
            KR_LOG_FATAL("CreateWindowExW 失敗 (エラーコード {})", GetLastError());
        }

        ShowWindow(m_Handle, SW_SHOW);
        KR_LOG_INFO("ウィンドウ作成 {}x{} (DPI {})", m_Width, m_Height, GetDpiForWindow(m_Handle));
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

    void Window::RequestClose()
    {
        if (m_Handle)
        {
            PostMessageW(m_Handle, WM_CLOSE, 0, 0); // 送るだけ(フレームの途中で、窓を壊さない)
        }
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
                    m_KeyDownCallback(ToKeyNumber(wParam, lParam));
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
                m_KeyUpCallback(ToKeyNumber(wParam, lParam));
            }

            // F10 は離したときもメニューを開く動きにつながるため、KEYDOWN と同じく既定の処理に渡さない
            if (wParam == VK_F10)
            {
                return 0;
            }
            break;
        }
        case WM_SYSCOMMAND:
        {
            // Alt(または F10)だけを押して離すと、Windows は「ウィンドウのメニューを選ぶ状態」に入り、メインループが止まる
            // Alt はゲームのキーなので、キーだけで入るこの状態(lParam が 0)を止める
            // (Alt+Space はメニューを開く。lParam に ' ' が入るので、こちらは止めない。Alt+F4 は別のメッセージなので影響しない)
            if ((wParam & 0xFFF0) == SC_KEYMENU && lParam == 0)
            {
                return 0;
            }
            break;
        }
        case WM_LBUTTONDOWN: OnMouseButton(VK_LBUTTON, true, wParam);  return 0;
        case WM_LBUTTONUP:   OnMouseButton(VK_LBUTTON, false, wParam); return 0;
        case WM_RBUTTONDOWN: OnMouseButton(VK_RBUTTON, true, wParam);  return 0;
        case WM_RBUTTONUP:   OnMouseButton(VK_RBUTTON, false, wParam); return 0;
        case WM_MBUTTONDOWN: OnMouseButton(VK_MBUTTON, true, wParam);  return 0;
        case WM_MBUTTONUP:   OnMouseButton(VK_MBUTTON, false, wParam); return 0;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        {
            // 横のボタンは 1 つのメッセージで届き、どちらのボタンかは wParam の上の 16 ビット
            const uint32_t virtualKey = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
            OnMouseButton(virtualKey, message == WM_XBUTTONDOWN, wParam);
            return TRUE; // 横のボタンだけは、処理したら TRUE を返す決まり
        }
        case WM_MOUSEMOVE:
        {
            if (m_MouseMoveCallback)
            {
                m_MouseMoveCallback(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            // 回した量は 1 目盛り = WHEEL_DELTA(120)。細かく回せるマウスは、120 より小さい値でも届く
            if (m_MouseWheelCallback)
            {
                m_MouseWheelCallback(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
            }
            return 0;
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

    void Window::OnMouseButton(uint32_t virtualKey, bool down, WPARAM wParam)
    {
        if (down)
        {
            // 押したまま、ウィンドウの外へ出て離しても、「離した」が届くように、マウスをこのウィンドウにつかまえておく
            SetCapture(m_Handle);
            if (m_MouseButtonDownCallback)
            {
                m_MouseButtonDownCallback(virtualKey);
            }
            return;
        }

        if (m_MouseButtonUpCallback)
        {
            m_MouseButtonUpCallback(virtualKey);
        }
        // wParam = 離したあとも押しているボタンの印。全部離したら、つかまえるのをやめる
        constexpr WPARAM AnyButton = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
        if ((wParam & AnyButton) == 0 && GetCapture() == m_Handle)
        {
            ReleaseCapture();
        }
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

        KR_LOG_INFO("表示モード: {}", m_Borderless ? "ボーダーレス" : "ウィンドウ");
    }
}
