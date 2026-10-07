#pragma once

#include <cstdint>

// 入力の状態表への書き込み口(エンジン内部専用。Game からは見えない)
// Window が OS からのメッセージを受けて呼び、Application がフレームの最後に EndFrame を呼ぶ
namespace KR::InputState
{
    // virtualKey は Windows のキー番号(WM_KEYDOWN の wParam)
    void OnKeyDown(uint32_t virtualKey);
    void OnKeyUp(uint32_t virtualKey);

    // ウィンドウが非アクティブになったとき、押しっぱなしのまま固まらないように全部離した扱いにする
    void ReleaseAll();

    // 「このフレームで押された・離された」の付箋を全部剥がす(毎フレームの最後に1回)
    void EndFrame();
}
