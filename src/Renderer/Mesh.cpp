#include "KR/Renderer/Mesh.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace DirectX;

namespace KR
{
    namespace
    {
        constexpr size_t FaceCount = 6;

        // 面ごとに専用の頂点を4つずつ持つ(同じ角でも面ごとに色を変えるため。4頂点 × 6面 = 24頂点)
        // 各面とも「その面を外側から見て 左上 → 右上 → 右下 → 左下」の順に並べる
        constexpr XMFLOAT3 CubeCorners[FaceCount][4] = {
            { { -0.5f,  0.5f, -0.5f }, {  0.5f,  0.5f, -0.5f }, {  0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f, -0.5f } }, // 前(-Z)
            { {  0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f,  0.5f }, { -0.5f, -0.5f,  0.5f }, {  0.5f, -0.5f,  0.5f } }, // 後(+Z)
            { {  0.5f,  0.5f, -0.5f }, {  0.5f,  0.5f,  0.5f }, {  0.5f, -0.5f,  0.5f }, {  0.5f, -0.5f, -0.5f } }, // 右(+X)
            { { -0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f, -0.5f }, { -0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f,  0.5f } }, // 左(-X)
            { { -0.5f,  0.5f,  0.5f }, {  0.5f,  0.5f,  0.5f }, {  0.5f,  0.5f, -0.5f }, { -0.5f,  0.5f, -0.5f } }, // 上(+Y)
            { { -0.5f, -0.5f, -0.5f }, {  0.5f, -0.5f, -0.5f }, {  0.5f, -0.5f,  0.5f }, { -0.5f, -0.5f,  0.5f } }, // 下(-Y)
        };

        // 面ごとの法線(その面が向いている方向)。CubeCorners と同じ面の順
        constexpr XMFLOAT3 CubeNormals[FaceCount] = {
            {  0.0f,  0.0f, -1.0f }, // 前(-Z)
            {  0.0f,  0.0f,  1.0f }, // 後(+Z)
            {  1.0f,  0.0f,  0.0f }, // 右(+X)
            { -1.0f,  0.0f,  0.0f }, // 左(-X)
            {  0.0f,  1.0f,  0.0f }, // 上(+Y)
            {  0.0f, -1.0f,  0.0f }, // 下(-Y)
        };

        MeshData BuildCube(const std::array<XMFLOAT4, FaceCount>& faceColors)
        {
            MeshData data;
            for (size_t face = 0; face < FaceCount; ++face)
            {
                const auto base = static_cast<uint32_t>(data.vertices.size());
                for (const XMFLOAT3& corner : CubeCorners[face])
                {
                    data.vertices.push_back({ corner, CubeNormals[face], faceColors[face] });
                }

                // 左上・右上・右下 / 左上・右下・左下 の2つの三角形(どちらも時計回り)
                const uint32_t faceIndices[] = { 0, 1, 2, 0, 2, 3 };
                for (uint32_t index : faceIndices)
                {
                    data.indices.push_back(base + index);
                }
            }
            return data;
        }
    }

    MeshData CreateCubeMeshData()
    {
        return BuildCube({
            XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), // 前: 赤
            XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f), // 後: 水色
            XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), // 右: 緑
            XMFLOAT4(0.5f, 0.0f, 1.0f, 1.0f), // 左: 紫
            XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), // 上: 青
            XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), // 下: 黄
        });
    }

    MeshData CreateCubeMeshData(const XMFLOAT4& color)
    {
        return BuildCube({ color, color, color, color, color, color });
    }

    MeshData CreateCheckerPlaneMeshData(uint32_t tilesX, uint32_t tilesZ, float tileSize, const XMFLOAT3& origin,
                                        const XMFLOAT4& colorA, const XMFLOAT4& colorB)
    {
        MeshData data;
        data.vertices.reserve(static_cast<size_t>(tilesX) * tilesZ * 4);
        data.indices.reserve(static_cast<size_t>(tilesX) * tilesZ * 6);

        constexpr XMFLOAT3 Up = { 0.0f, 1.0f, 0.0f };

        for (uint32_t tz = 0; tz < tilesZ; ++tz)
        {
            for (uint32_t tx = 0; tx < tilesX; ++tx)
            {
                // タイルの4つの角(X・Z が小さい側 = 0、大きい側 = 1)
                const float x0 = origin.x + static_cast<float>(tx) * tileSize;
                const float x1 = x0 + tileSize;
                const float z0 = origin.z + static_cast<float>(tz) * tileSize;
                const float z1 = z0 + tileSize;

                // 隣り合うタイルで、色が交互になる
                const XMFLOAT4& color = ((tx + tz) % 2 == 0) ? colorA : colorB;

                // 真上から見て 左上(X 小・Z 大)→ 右上 → 右下 → 左下 の順(キューブの上の面と同じ。時計回りが表)
                const auto base = static_cast<uint32_t>(data.vertices.size());
                data.vertices.push_back({ { x0, origin.y, z1 }, Up, color });
                data.vertices.push_back({ { x1, origin.y, z1 }, Up, color });
                data.vertices.push_back({ { x1, origin.y, z0 }, Up, color });
                data.vertices.push_back({ { x0, origin.y, z0 }, Up, color });

                const uint32_t tileIndices[] = { 0, 1, 2, 0, 2, 3 };
                for (uint32_t index : tileIndices)
                {
                    data.indices.push_back(base + index);
                }
            }
        }
        return data;
    }

    MeshData CreateSphereMeshData(uint32_t rings, uint32_t segments, const XMFLOAT4& color)
    {
        // 少なすぎると球にならない(縦 2・横 3 が最低限)
        rings = std::max(rings, 2u);
        segments = std::max(segments, 3u);

        constexpr float Radius = 0.5f; // 直径 1 メートル(キューブと同じ大きさ)
        constexpr float Pi = 3.14159265358979f;

        MeshData data;
        data.vertices.reserve(static_cast<size_t>(rings + 1) * (segments + 1));
        data.indices.reserve(static_cast<size_t>(rings) * segments * 6);

        // 頂点:上(北極)から下(南極)へ rings + 1 段。1 段に segments + 1 個(つなぎ目は、先頭と同じ位置をもう 1 個持つ = UV を 0 → 1 にするため)
        for (uint32_t r = 0; r <= rings; ++r)
        {
            const float v = static_cast<float>(r) / static_cast<float>(rings);
            const float theta = Pi * v; // 0 = 真上、π = 真下
            const float y = std::cos(theta);
            const float ringRadius = std::sin(theta);
            for (uint32_t s = 0; s <= segments; ++s)
            {
                const float u = static_cast<float>(s) / static_cast<float>(segments);
                const float phi = 2.0f * Pi * u;
                const XMFLOAT3 normal = { ringRadius * std::cos(phi), y, ringRadius * std::sin(phi) }; // 中心から外への向き(長さ 1)
                data.vertices.push_back({ { normal.x * Radius, normal.y * Radius, normal.z * Radius }, normal, color, { u, v } });
            }
        }

        // 三角形:外側から見て時計回り。極の段は、1 つが潰れた三角形(面積 0)になるだけで、描画には影響しない
        const uint32_t stride = segments + 1;
        for (uint32_t r = 0; r < rings; ++r)
        {
            for (uint32_t s = 0; s < segments; ++s)
            {
                const uint32_t a = r * stride + s; // 上の段・この列
                const uint32_t b = a + 1;          // 上の段・次の列
                const uint32_t c = a + stride;     // 下の段・この列
                const uint32_t d = c + 1;          // 下の段・次の列
                const uint32_t quad[] = { a, b, c, b, d, c };
                data.indices.insert(data.indices.end(), std::begin(quad), std::end(quad));
            }
        }
        return data;
    }
}
