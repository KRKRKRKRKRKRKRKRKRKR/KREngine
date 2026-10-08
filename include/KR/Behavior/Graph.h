#pragma once

#include <map>
#include <string>
#include <variant>

namespace KR::Behavior
{
    // ノードが持てる値 1 つ:小数(float)・整数(int)・真偽(bool)・文字(string)のどれか
    // 例:速さ = float・ダメージ = int・縦も含める = bool・状況のキー名 = string
    using ParamValue = std::variant<float, int, bool, std::string>;

    // ノードの値の一覧:名前 → 値。名前の順に並ぶ(JSON に書いたとき、順番が、いつも同じになる)
    using ParamMap = std::map<std::string, ParamValue>;
}
