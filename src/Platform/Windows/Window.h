#pragma once

#include <Windows.h>

#include <cstdint>
#include <functional>
#include <string>

namespace TenFiftyEight
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

        HWND m_Handle = nullptr;
        HINSTANCE m_Instance = nullptr;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        ResizeCallback m_ResizeCallback;
        KeyDownCallback m_KeyDownCallback;
        KeyUpCallback m_KeyUpCallback;
        FocusLostCallback m_FocusLostCallback;
        MessageHook m_MessageHook;

        bool m_Borderless = false;
        WINDOWPLACEMENT m_WindowedPlacement = { sizeof(WINDOWPLACEMENT) }; // ウィンドウ表示に戻すときの位置・サイズ
    };
}
