#pragma once

#include "KR/Animation/Skeleton.h"
#include "KR/Renderer/Renderer.h"

#include <DirectXMath.h>

#include <vector>

namespace KR
{
    // 基準姿勢での、各骨の世界の行列(モデルの座標。Skeleton::joints と同じ並び)
    // 逆バインド行列の逆行列(= 基準姿勢での、骨の世界の行列)で求める
    std::vector<DirectX::XMFLOAT4X4> GetBindPoseWorldMatrices(const Skeleton& skeleton);

    // 骨を、Blender のような形(八面体:根もとの近くが太く、先に向かって細くなる)で描く(デバッグ用)
    //   骨 1 本 = 親の関節から、子の関節まで。子の関節が複数あれば、その数だけ、骨が出る。子の無い関節(先端)には、短い骨(親の骨の 3 割の長さ)
    //   塗りつぶした形(光で陰影がつく)+ 縁の線。選んだ骨は、色を変えて、目立たせる
    //   使い方:Create(最初に 1 回)→ 毎フレーム Draw → Destroy(最後に 1 回)
    class BoneRenderer
    {
    public:
        struct Style
        {
            DirectX::XMFLOAT4 outlineColor = { 0.05f, 0.05f, 0.08f, 1.0f }; // 縁の線の色
            int selectedJoint = -1; // 目立たせる骨(その関節から出る骨。-1 = なし)
            bool inFront = true;    // true: 骨を、ほかの物(メッシュなど)の手前に描く(Blender の「In Front」。物の中の骨も、見える)
        };

        // 形(通常の色・選んだ色)を作る。色は、頂点に持たせる(形ごとに別の色なので、2 つ作る)
        void Create(Renderer& renderer, const DirectX::XMFLOAT4& color = { 0.62f, 0.68f, 0.80f, 1.0f },
                    const DirectX::XMFLOAT4& selectedColor = { 1.0f, 0.6f, 0.1f, 1.0f });
        void Destroy(Renderer& renderer);

        // jointWorld は、各骨の世界の行列(GetBindPoseWorldMatrices や EvaluatePose の結果)。world は、モデル全体の置き方
        void Draw(Renderer& renderer, const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& jointWorld,
                  const DirectX::XMMATRIX& world, const Style& style) const;

    private:
        MeshHandle m_Normal;   // 通常の骨の形(長さ 1・根もとが原点・先が +Y)
        MeshHandle m_Selected; // 選んだ骨の形(色だけ違う)
    };
}
