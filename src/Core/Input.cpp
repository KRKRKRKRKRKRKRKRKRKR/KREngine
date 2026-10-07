#include "TenFiftyEight/Core/Input.h"

#include "Core/InputState.h"

#include <Windows.h>

#include <array>

namespace TenFiftyEight
{
    namespace
    {
        constexpr size_t KeyCount = 256; // Windows のキー番号は 0〜255

        // 掲示板(キー番号ごとの状態)
        std::array<bool, KeyCount> g_Held = {};     // 今押している
        std::array<bool, KeyCount> g_Pressed = {};  // このフレームで押された(付箋)
        std::array<bool, KeyCount> g_Released = {}; // このフレームで離された(付箋)

        // Game 向けの Key を、Windows のキー番号に直す(アルファベットのキー番号は大文字の文字コードと同じ)
        uint32_t ToVirtualKey(Key key)
        {
            switch (key)
            {
            case Key::W:     return 'W';
            case Key::A:     return 'A';
            case Key::S:     return 'S';
            case Key::D:     return 'D';
            case Key::Left:  return VK_LEFT;
            case Key::Right: return VK_RIGHT;
            case Key::Up:    return VK_UP;
            case Key::Down:  return VK_DOWN;
            case Key::R:     return 'R';
            case Key::Enter: return VK_RETURN;
            case Key::F3:    return VK_F3;
            }
            return 0;
        }
    }

    bool Input::IsKeyDown(Key key)
    {
        return g_Held[ToVirtualKey(key)];
    }

    bool Input::IsKeyPressed(Key key)
    {
        return g_Pressed[ToVirtualKey(key)];
    }

    bool Input::IsKeyReleased(Key key)
    {
        return g_Released[ToVirtualKey(key)];
    }

    namespace InputState
    {
        void OnKeyDown(uint32_t virtualKey)
        {
            if (virtualKey >= KeyCount)
            {
                return;
            }
            // 押しっぱなしで同じメッセージが繰り返し届いても、すでに押していれば「押された」にはしない
            if (!g_Held[virtualKey])
            {
                g_Pressed[virtualKey] = true;
            }
            g_Held[virtualKey] = true;
        }

        void OnKeyUp(uint32_t virtualKey)
        {
            if (virtualKey >= KeyCount)
            {
                return;
            }
            if (g_Held[virtualKey])
            {
                g_Released[virtualKey] = true;
            }
            g_Held[virtualKey] = false;
        }

        void ReleaseAll()
        {
            for (uint32_t key = 0; key < KeyCount; ++key)
            {
                OnKeyUp(key);
            }
        }

        void EndFrame()
        {
            g_Pressed.fill(false);
            g_Released.fill(false);
        }
    }
}
