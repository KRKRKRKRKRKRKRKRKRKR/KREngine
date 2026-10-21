#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace KR
{
    // 頂点1つ分(シェーダーの VSInput と形を合わせる)
    struct Vertex
    {
        DirectX::XMFLOAT3 position; // 位置(メートル)
        DirectX::XMFLOAT3 normal;   // 法線(面が向いている方向。長さ 1 の矢印。陰影の計算に使う)
        DirectX::XMFLOAT4 color;    // 色(r, g, b, a)
        DirectX::XMFLOAT2 uv = { 0.0f, 0.0f }; // 画像の、どこの色を使うか(u = 横、v = 縦。0 〜 1。左上が (0, 0))。画像を使わない形は (0, 0)
    };

    // 骨に引っぱられる設定(頂点1つ分)。スキニング用。1 頂点に、最大 4 本の骨
    struct SkinVertex
    {
        uint16_t joints[4] = { 0, 0, 0, 0 };         // 骨の番号(Skeleton::joints の何番目か)。使わない枠は、重み 0
        float weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f }; // 重み(どれだけ引っぱられるか。4 つの合計が 1)
    };

    // 形のデータ(CPU 側)。CreateMesh に渡したら、Game 側では捨ててよい
    struct MeshData
    {
        std::vector<Vertex> vertices;  // 頂点リスト
        std::vector<uint32_t> indices; // 番号リスト(3つで三角形1つ。外側から見て時計回り)

        // 骨の設定。空なら、骨なしの形。空でなければ、vertices と同じ数(同じ並び)。骨つきの形は DrawSkinnedMesh で描く
        std::vector<SkinVertex> skin;

        // 貼る画像のファイル(.mtl の map_Kd。フルパス)。無ければ空。LoadObj が入れる
        std::filesystem::path diffuseTexturePath;
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

    // 市松模様の平らな板(上向き)。tileSize メートル四方のタイルを、X 方向に tilesX 枚、Z 方向に tilesZ 枚並べ、
    // colorA と colorB を交互に塗る。origin は、板の「X・Z が最小の角」の位置(Y が板の高さ)
    // 頂点の法線は、すべて上向き (0, 1, 0)
    MeshData CreateCheckerPlaneMeshData(uint32_t tilesX, uint32_t tilesZ, float tileSize,
                                        const DirectX::XMFLOAT3& origin,
                                        const DirectX::XMFLOAT4& colorA, const DirectX::XMFLOAT4& colorB);

    // 球。直径 1 メートル(キューブと同じ大きさ)・中心が原点。rings = 縦の分割(上から下)・segments = 横の分割(ぐるっと一周)。全部の頂点が同じ色
    // 法線は、中心から外への向き(なめらかな陰影)。UV は、横が u・縦が v(0 〜 1)。目安は 16 × 32(滑らかさが足りなければ増やす)
    MeshData CreateSphereMeshData(uint32_t rings, uint32_t segments, const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
}
