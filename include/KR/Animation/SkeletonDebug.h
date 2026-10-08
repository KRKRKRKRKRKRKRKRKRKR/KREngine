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

    // 骨を、線で描く(デバッグ用)。親の骨の位置から、子の骨の位置へ、1本ずつ。根の骨(親がいない)には、小さな十字
    // jointWorld は、各骨の世界の行列(GetBindPoseWorldMatrices の結果など)。world は、モデル全体の置き方(位置・回転)
    // 線は、物に隠れる(Renderer::DrawLine と同じ)。メッシュの中の骨を見たいときは、メッシュを描かない
    void DrawSkeleton(Renderer& renderer, const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& jointWorld,
                      const DirectX::XMMATRIX& world, const DirectX::XMFLOAT4& color);
}
