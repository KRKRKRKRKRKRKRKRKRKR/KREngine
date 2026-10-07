#pragma once

namespace TenFiftyEight
{
    // Game が使えるキー(Windows のキー番号は Input.cpp の中に閉じ込めて、Game に見せない)
    enum class Key
    {
        W, A, S, D,
        Left, Right, Up, Down,
        R, Enter, // ゲームオーバーのあと、やり直す
        F3, // デバッグ用の画面(ImGui)の表示切り替え
    };

    // キーの状態を「読む」ための入口。書き込み(押した・離した)はエンジン内部だけが行う
    // 状態表は毎フレームの最後に更新される(Application のメインループ)
    class Input
    {
    public:
        // 今、押されている(押された瞬間のフレームも true)
        static bool IsKeyDown(Key key);

        // このフレームで押された(押しっぱなしにしても、最初の1フレームだけ true)
        static bool IsKeyPressed(Key key);

        // このフレームで離された(1フレームだけ true)
        static bool IsKeyReleased(Key key);
    };
}
