#pragma once

namespace KR
{
    // Game が使えるキーとマウスのボタン(Windows のキー番号は Input.cpp の中に閉じ込めて、Game に見せない)
    // マウスのボタンも、キーと同じ列挙に入れる(IsKeyDown がそのまま使える。将来、キーの割り当てを変える設定で、同じように扱える)
    // 並びは、Input.cpp の対応表(KeyTable)と同じ順番にする。足すときは、Count の前に足して、対応表にも 1 行
    enum class Key
    {
        // 文字のキー
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        // 数字のキー(上の段。テンキーは Numpad0〜9)
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

        // ファンクションキー(F10・F11 はエンジンも使う。F10 = シェーダーの再読み込み(Debug)、F11 = ボーダーレスの切り替え)
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        // 矢印
        Left, Right, Up, Down,

        // よく使うキー
        Enter, Space, Escape, Tab, Backspace,

        // 組み合わせのキー(左右を区別しない。どちらを押しても同じ)
        Shift, Ctrl, Alt,

        // 編集のキー
        Insert, Delete, Home, End, PageUp, PageDown,

        // テンキー(NumLock が ON のとき。OFF のときは、Windows が Insert・矢印などとして送ってくる)
        Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
        NumpadAdd, NumpadSubtract, NumpadMultiply, NumpadDivide, NumpadDecimal,
        NumpadEnter, // 上の Enter とは別(Enter を押しても NumpadEnter は true にならない)

        // マウスのボタン(X1・X2 = 横の「戻る」「進む」ボタン)
        MouseLeft, MouseRight, MouseMiddle, MouseX1, MouseX2,

        Count, // キーの数(キーではない。全部のキーを順に調べるときに使う)
    };

    // マウスの位置(ウィンドウの描画される部分の左上が (0, 0)。右が x、下が y。単位は画面のピクセル)
    struct MousePosition
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    // キーの状態を「読む」ための入口。書き込み(押した・離した)はエンジン内部だけが行う
    // 状態表は毎フレームの最後に更新される(Application のメインループ)
    // ImGui の窓を使っている間(文字入力・窓の上のクリック)は、「押した」はゲームに届かない
    class Input
    {
    public:
        // 今、押されている(押された瞬間のフレームも true)
        static bool IsKeyDown(Key key);

        // このフレームで押された(押しっぱなしにしても、最初の1フレームだけ true)
        static bool IsKeyPressed(Key key);

        // このフレームで離された(1フレームだけ true)
        static bool IsKeyReleased(Key key);

        // キーの名前("A"・"Space"・"MouseLeft" など。デバッグの表示・キーの設定の画面用)
        static const char* GetKeyName(Key key);

        // マウスの最後の位置。Camera::ScreenPointToRay(x, y, 幅, 高さ) に、そのまま渡せる
        // ボタンを押したまま、ウィンドウの外へ出ると、範囲の外(負の値・幅より大きい値)にもなる
        static MousePosition GetMousePosition();

        // このフレームで回したホイールの量(1 = 1 目盛り。奥へ回すと +、手前へ回すと -)
        static float GetMouseWheel();
    };
}
