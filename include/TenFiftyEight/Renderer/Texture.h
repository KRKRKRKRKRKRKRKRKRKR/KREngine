#pragma once

#include <cstdint>

namespace TenFiftyEight
{
    // 画像(テクスチャ)の番号札。実体(GPU のメモリ)は Renderer が預かっている。MeshHandle と同じ考え方
    struct TextureHandle
    {
        uint32_t id = 0; // 0 = 無効(まだ CreateTexture していない)

        bool IsValid() const { return id != 0; }
    };
}
