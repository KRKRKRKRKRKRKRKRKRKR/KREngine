#pragma once

#include <Windows.h>

#include <cstdint>
#include <functional>
#include <string>

namespace KR
{
    struct WindowDesc
    {
        std::string title;
        uint32_t width = 0;  // クライアント領域(描画される部分)のサイズ。物理ピクセル
        uint32_t height = 0;
    };

    class Window
    {
    public:
        using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
        using KeyDownCallback = std::function<void(uint32_t virtualKey)>; // virtualKey は VK_F10 などの Windows のキーコード
        using KeyUpCallback = std::function<void(uint32_t virtualKey)>;
        using FocusLostCallback = std::function<void()>; // 別のウィンドウに切り替わるなどで、非アクティブになった
        // マウスのボタン。virtualKey は VK_LBUTTON・VK_RBUTTON・VK_MBUTTON・VK_XBUTTON1・VK_XBUTTON2(キーと同じ番号の表で扱う)
        using MouseButtonDownCallback = std::function<void(uint32_t virtualKey)>;
        using MouseButtonUpCallback = std::function<void(uint32_t virtualKey)>;
        using MouseMoveCallback = std::function<void(int32_t x, int32_t y)>; // 描画される部分の左上が (0, 0)。ピクセル
        using MouseWheelCallback = std::function<void(float notches)>;       // 1 = 1 目盛り。奥へ回すと +

        // OS のメッセージを、Window 自身が処理する前に見せる窓口。処理した(以降は Window が触らない)なら true を返す
        using MessageHook = std::function<bool(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)>;

        explicit Window(const WindowDesc& desc);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        // たまっているメッセージをすべて処理する。終了要求(WM_QUIT)が来たら false を返す
        bool ProcessMessages();

        void SetResizeCallback(ResizeCallback callback) { m_ResizeCallback = std::move(callback); }
        void SetKeyDownCallback(KeyDownCallback callback) { m_KeyDownCallback = std::move(callback); }
        void SetKeyUpCallback(KeyUpCallback callback) { m_KeyUpCallback = std::move(callback); }
        void SetFocusLostCallback(FocusLostCallback callback) { m_FocusLostCallback = std::move(callback); }
        void SetMouseButtonDownCallback(MouseButtonDownCallback callback) { m_MouseButtonDownCallback = std::move(callback); }
        void SetMouseButtonUpCallback(MouseButtonUpCallback callback) { m_MouseButtonUpCallback = std::move(callback); }
        void SetMouseMoveCallback(MouseMoveCallback callback) { m_MouseMoveCallback = std::move(callback); }
        void SetMouseWheelCallback(MouseWheelCallback callback) { m_MouseWheelCallback = std::move(callback); }
        void SetMessageHook(MessageHook hook) { m_MessageHook = std::move(hook); }

        HWND GetHandle() const { return m_Handle; }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

    private:
        // OS から呼ばれる関数は static である必要があるため、ここで this を取り出して HandleMessage に渡す
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

        // ウィンドウ ⇔ ボーダーレス(枠なしでモニター全体を覆う)を切り替える
        void ToggleBorderless();

        // マウスのボタンを押した・離したメッセージを、コールバックに渡す(押している間は、ウィンドウの外のマウスも受け取る)
        void OnMouseButton(uint32_t virtualKey, bool down, WPARAM wParam);

        HWND m_Handle = nullptr;
        HINSTANCE m_Instance = nullptr;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        ResizeCallback m_ResizeCallback;
        KeyDownCallback m_KeyDownCallback;
        KeyUpCallback m_KeyUpCallback;
        FocusLostCallback m_FocusLostCallback;
        MouseButtonDownCallback m_MouseButtonDownCallback;
        MouseButtonUpCallback m_MouseButtonUpCallback;
        MouseMoveCallback m_MouseMoveCallback;
        MouseWheelCallback m_MouseWheelCallback;
        MessageHook m_MessageHook;

        bool m_Borderless = false;
        WINDOWPLACEMENT m_WindowedPlacement = { sizeof(WINDOWPLACEMENT) }; // ウィンドウ表示に戻すときの位置・サイズ
    };
}
