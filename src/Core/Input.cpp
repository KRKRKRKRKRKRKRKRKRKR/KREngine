#include "KR/Core/Input.h"

#include "Core/InputState.h"
#include "Platform/Windows/KeyCodes.h"

#include <Windows.h>

#include <array>

namespace KR
{
    namespace
    {
        constexpr size_t KeyCount = 256; // Windows のキー番号は 0〜255

        // 掲示板(キー番号ごとの状態)。マウスのボタンも、Windows のキー番号(VK_LBUTTON など)で、同じ表に入れる
        std::array<bool, KeyCount> g_Held = {};     // 今押している
        std::array<bool, KeyCount> g_Pressed = {};  // このフレームで押された(付箋)
        std::array<bool, KeyCount> g_Released = {}; // このフレームで離された(付箋)

        MousePosition g_MousePosition;  // 最後に届いたマウスの位置
        float g_MouseWheel = 0.0f;      // このフレームで回したホイールの量(付箋と同じく、フレームの最後に 0 に戻す)

        // Game 向けの Key と、Windows のキー番号・名前の対応表。並びは Key の列挙と同じ順番
        // (アルファベットと数字のキー番号は、大文字の文字コードと同じ)
        struct KeyInfo
        {
            Key key;             // 並びの確認用(下の static_assert)
            uint32_t virtualKey; // Windows のキー番号
            const char* name;
        };

        constexpr std::array<KeyInfo, static_cast<size_t>(Key::Count)> KeyTable = { {
            { Key::A, 'A', "A" }, { Key::B, 'B', "B" }, { Key::C, 'C', "C" }, { Key::D, 'D', "D" },
            { Key::E, 'E', "E" }, { Key::F, 'F', "F" }, { Key::G, 'G', "G" }, { Key::H, 'H', "H" },
            { Key::I, 'I', "I" }, { Key::J, 'J', "J" }, { Key::K, 'K', "K" }, { Key::L, 'L', "L" },
            { Key::M, 'M', "M" }, { Key::N, 'N', "N" }, { Key::O, 'O', "O" }, { Key::P, 'P', "P" },
            { Key::Q, 'Q', "Q" }, { Key::R, 'R', "R" }, { Key::S, 'S', "S" }, { Key::T, 'T', "T" },
            { Key::U, 'U', "U" }, { Key::V, 'V', "V" }, { Key::W, 'W', "W" }, { Key::X, 'X', "X" },
            { Key::Y, 'Y', "Y" }, { Key::Z, 'Z', "Z" },

            { Key::Num0, '0', "0" }, { Key::Num1, '1', "1" }, { Key::Num2, '2', "2" }, { Key::Num3, '3', "3" },
            { Key::Num4, '4', "4" }, { Key::Num5, '5', "5" }, { Key::Num6, '6', "6" }, { Key::Num7, '7', "7" },
            { Key::Num8, '8', "8" }, { Key::Num9, '9', "9" },

            { Key::F1, VK_F1, "F1" }, { Key::F2, VK_F2, "F2" }, { Key::F3, VK_F3, "F3" }, { Key::F4, VK_F4, "F4" },
            { Key::F5, VK_F5, "F5" }, { Key::F6, VK_F6, "F6" }, { Key::F7, VK_F7, "F7" }, { Key::F8, VK_F8, "F8" },
            { Key::F9, VK_F9, "F9" }, { Key::F10, VK_F10, "F10" }, { Key::F11, VK_F11, "F11" }, { Key::F12, VK_F12, "F12" },

            { Key::Left, VK_LEFT, "Left" }, { Key::Right, VK_RIGHT, "Right" },
            { Key::Up, VK_UP, "Up" }, { Key::Down, VK_DOWN, "Down" },

            { Key::Enter, VK_RETURN, "Enter" }, { Key::Space, VK_SPACE, "Space" }, { Key::Escape, VK_ESCAPE, "Escape" },
            { Key::Tab, VK_TAB, "Tab" }, { Key::Backspace, VK_BACK, "Backspace" },

            // Windows は、左右どちらでも VK_SHIFT・VK_CONTROL・VK_MENU(Alt)で送ってくる
            { Key::Shift, VK_SHIFT, "Shift" }, { Key::Ctrl, VK_CONTROL, "Ctrl" }, { Key::Alt, VK_MENU, "Alt" },

            { Key::Insert, VK_INSERT, "Insert" }, { Key::Delete, VK_DELETE, "Delete" },
            { Key::Home, VK_HOME, "Home" }, { Key::End, VK_END, "End" },
            { Key::PageUp, VK_PRIOR, "PageUp" }, { Key::PageDown, VK_NEXT, "PageDown" },

            { Key::Numpad0, VK_NUMPAD0, "Numpad0" }, { Key::Numpad1, VK_NUMPAD1, "Numpad1" },
            { Key::Numpad2, VK_NUMPAD2, "Numpad2" }, { Key::Numpad3, VK_NUMPAD3, "Numpad3" },
            { Key::Numpad4, VK_NUMPAD4, "Numpad4" }, { Key::Numpad5, VK_NUMPAD5, "Numpad5" },
            { Key::Numpad6, VK_NUMPAD6, "Numpad6" }, { Key::Numpad7, VK_NUMPAD7, "Numpad7" },
            { Key::Numpad8, VK_NUMPAD8, "Numpad8" }, { Key::Numpad9, VK_NUMPAD9, "Numpad9" },
            { Key::NumpadAdd, VK_ADD, "NumpadAdd" }, { Key::NumpadSubtract, VK_SUBTRACT, "NumpadSubtract" },
            { Key::NumpadMultiply, VK_MULTIPLY, "NumpadMultiply" }, { Key::NumpadDivide, VK_DIVIDE, "NumpadDivide" },
            { Key::NumpadDecimal, VK_DECIMAL, "NumpadDecimal" },
            { Key::NumpadEnter, KeyCodes::NumpadEnter, "NumpadEnter" }, // Windows の番号が無いので、エンジンが付けた番号

            { Key::MouseLeft, VK_LBUTTON, "MouseLeft" }, { Key::MouseRight, VK_RBUTTON, "MouseRight" },
            { Key::MouseMiddle, VK_MBUTTON, "MouseMiddle" },
            { Key::MouseX1, VK_XBUTTON1, "MouseX1" }, { Key::MouseX2, VK_XBUTTON2, "MouseX2" },
        } };

        // 対応表の並びが、Key の列挙と同じ順番か、コンパイルのときに確かめる(ずれていると、ビルドが止まる)
        constexpr bool IsKeyTableInOrder()
        {
            for (size_t i = 0; i < KeyTable.size(); ++i)
            {
                if (static_cast<size_t>(KeyTable[i].key) != i)
                {
                    return false;
                }
            }
            return true;
        }
        static_assert(IsKeyTableInOrder(), "KeyTable の並びが、Key の列挙と違う(Input.h と Input.cpp を見直す)");

        // Game 向けの Key を、Windows のキー番号に直す。範囲の外(Key::Count など)は 0(どのキーでもない番号)
        uint32_t ToVirtualKey(Key key)
        {
            const size_t index = static_cast<size_t>(key);
            return index < KeyTable.size() ? KeyTable[index].virtualKey : 0;
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

    const char* Input::GetKeyName(Key key)
    {
        const size_t index = static_cast<size_t>(key);
        return index < KeyTable.size() ? KeyTable[index].name : "";
    }

    MousePosition Input::GetMousePosition()
    {
        return g_MousePosition;
    }

    float Input::GetMouseWheel()
    {
        return g_MouseWheel;
    }

    namespace InputState
    {
        void OnKeyDown(uint32_t virtualKey)
        {
            // 0 は「どのキーでもない」番号(ToVirtualKey が範囲の外で返す)。書き込まない
            if (virtualKey == 0 || virtualKey >= KeyCount)
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

        void OnMouseMove(int32_t x, int32_t y)
        {
            g_MousePosition = { static_cast<float>(x), static_cast<float>(y) };
        }

        void OnMouseWheel(float notches)
        {
            g_MouseWheel += notches;
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
            g_MouseWheel = 0.0f;
        }
    }
}
