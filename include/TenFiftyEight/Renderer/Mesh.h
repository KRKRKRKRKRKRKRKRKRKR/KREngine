#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <vector>

namespace TenFiftyEight
{
    // 頂点1つ分(シェーダーの VSInput と形を合わせる)
    struct Vertex
    {
        DirectX::XMFLOAT3 position; // 位置(メートル)
        DirectX::XMFLOAT4 color;    // 色(r, g, b, a)
    };

    // 形のデータ(CPU 側)。CreateMesh に渡したら、Game 側では捨ててよい
    struct MeshData
    {
        std::vector<Vertex> vertices;  // 頂点リスト
        std::vector<uint16_t> indices; // 番号リスト(3つで三角形1つ。外側から見て時計回り)
    };

    // 形の番号札。実体(GPU のバッファ)は Renderer が預かっている
    struct MeshHandle
    {
        uint32_t id = 0; // 0 = 無効(まだ CreateMesh していない)

        bool IsValid() const { return id != 0; }
    };

    // 1辺1メートル、中心が原点のキューブ。面ごとに色が違う(前:赤、後:水色、右:緑、左:紫、上:青、下:黄)
    MeshData CreateCubeMeshData();

    // 1辺1メートル、中心が原点のキューブ。全部の面が同じ色
    MeshData CreateCubeMeshData(const DirectX::XMFLOAT4& color);
}
