#pragma once

#include "KR/Animation/Skeleton.h"

#include <DirectXMath.h>

#include <vector>

namespace KR
{
    // スキニング用の、骨の行列の表(パレット)を作る。骨ごとに「逆バインド行列 × 骨の今の世界の行列」
    //   逆バインド行列で、頂点を「その骨の中の座標」に戻し、骨の今の行列で、動いたあとの位置に置く(基準姿勢のときは、打ち消し合って、動かない)
    // pose は、EvaluatePose の結果(骨の今の世界の行列。Skeleton::joints と同じ並び)
    // 結果は、Renderer::DrawSkinnedMesh の palette に、そのまま渡せる
    std::vector<DirectX::XMFLOAT4X4> ComputeSkinMatrices(const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& pose);
}
