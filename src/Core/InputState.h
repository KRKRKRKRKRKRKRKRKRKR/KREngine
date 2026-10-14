#pragma once

#include <cstdint>

// 入力の状態表への書き込み口(エンジン内部専用。Game からは見えない)
// Window が OS からのメッセージを受けて呼び、Application がフレームの最後に EndFrame を呼ぶ
namespace KR::InputState
{
    // virtualKey は Windows のキー番号(WM_KEYDOWN の wParam。マウスのボタンは VK_LBUTTON など)
    // Windows の番号が無いキーは、エンジンが付けた番号(Platform/Windows/KeyCodes.h)
    void OnKeyDown(uint32_t virtualKey);
    void OnKeyUp(uint32_t virtualKey);

    // マウスの位置(ウィンドウの描画される部分の左上が (0, 0)。ピクセル)
    void OnMouseMove(int32_t x, int32_t y);

    // ホイールを回した量(1 = 1 目盛り)。同じフレームに何回届いても、足していく
    void OnMouseWheel(float notches);

    // ウィンドウが非アクティブになったとき、押しっぱなしのまま固まらないように全部離した扱いにする
    void ReleaseAll();

    // 「このフレームで押された・離された」の付箋と、ホイールの量を全部剥がす(毎フレームの最後に1回)
    void EndFrame();
}
