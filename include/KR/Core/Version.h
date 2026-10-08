#pragma once

#include <string>

namespace KR
{
    // エンジンの版:メジャー.マイナー.パッチ(例 0.2.1)。決まりは docs/Versioning.md・更新の記録は CHANGELOG.md
    //   パッチ:バグ修正だけ(使い方は変わらない)
    //   マイナー:機能の追加(今までの使い方は、そのまま動く)
    //   メジャー:互換性が壊れる変更(ゲームのコードを直す必要がある)。0 の間は、開発中(マイナーでも、互換性が変わることがある)
    struct Version
    {
        int major = 0;
        int minor = 0;
        int patch = 0;
    };

    // このエンジンをビルドしたときの版(CMakeLists.txt の project(KREngine VERSION ...) が、元)
    Version GetVersion();

    // "0.2.1" の形の文字列
    std::string GetVersionString();
}
