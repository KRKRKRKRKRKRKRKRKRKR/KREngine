#include "TenFiftyEight/Renderer/Mesh.h"

#include <array>

using namespace DirectX;

namespace TenFiftyEight
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
}
