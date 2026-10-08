#pragma once

#include <DirectXMath.h>

#include <string>
#include <vector>

namespace KR
{
    // 骨(関節)1本分
    struct Joint
    {
        std::string name;
        int parent = -1; // 親の骨の番号(Skeleton::joints の何番目か)。-1 = 根(親が骨ではない)。親が、子より後ろに並ぶこともある

        // 根(parent が -1)のときだけ使う:親のノード(骨ではない。例:Blender のアーマチュア)の、世界の行列。
        // 骨の世界の行列 = 自分の行列 × 親の世界の行列。根は、この行列を掛けて、モデルの座標に置く。ノードが無ければ、単位行列
        DirectX::XMFLOAT4X4 parentWorld = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

        // 基準姿勢(バインドポーズ。何も再生していないときの姿勢)。親の骨から見た、位置・向き・大きさ
        DirectX::XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f }; // クォータニオン(x, y, z, w)。向きを 4 つの数で表す
        DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };

        // 逆バインド行列:基準姿勢での、この骨の世界の行列の逆。「形の頂点を、この骨の中の座標に戻す」ために使う(スキニングで使う)
        DirectX::XMFLOAT4X4 inverseBind = {};
    };

    // 骨の階層全体。joints の並びは、glTF のスキンの並び(頂点が持つ「骨の番号」は、この並びの番号)
    struct Skeleton
    {
        std::vector<Joint> joints;

        bool IsEmpty() const { return joints.empty(); }
    };
}
