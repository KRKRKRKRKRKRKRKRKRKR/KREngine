#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace TenFiftyEight
{
    // 画像を、ほどいた(デコードした)もの。GPU が読めるのは、圧縮されていない、ピクセルの色の並びだけ
    struct ImageData
    {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> pixels; // 1 ピクセル 4 バイト(R, G, B, A)。左上から、右へ・下へ。大きさ = width × height × 4

        bool IsValid() const { return !pixels.empty(); }
    };

    // PNG などの画像ファイルを、ほどく。元が RGB でも、必ず 4 つ(RGBA)にそろえる(A が無ければ 255)
    // 失敗したとき(ファイルが開けない・画像として読めない)は、エラーをログに出して、空(IsValid() が false)を返す
    // (名前が LoadImage でないのは、Windows のヘッダーの LoadImage(マクロ)とぶつかるため)
    ImageData LoadImageRgba8(const std::filesystem::path& file);
}
