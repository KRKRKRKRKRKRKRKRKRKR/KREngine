#pragma once

#include <cstdint>

// Windows のキー番号(VK_〜)に無いキーに、エンジンが付ける番号(エンジン内部専用)
// Window がメッセージを読んで付け、Input.cpp の対応表が読む。どちらも Windows に依存する場所なので、ここに置く
namespace KR::KeyCodes
{
    // テンキーの Enter。Windows は、上の Enter と同じ VK_RETURN で送ってくる(違いは lParam の「拡張キー」の印だけ)
    // 0x0E は、Windows のキー番号で、使われていない番号
    constexpr uint32_t NumpadEnter = 0x0E;
}
