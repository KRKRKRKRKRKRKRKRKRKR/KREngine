#include "KR/Animation/SkinMatrices.h"

#include <algorithm>

using namespace DirectX;

namespace KR
{
    std::vector<XMFLOAT4X4> ComputeSkinMatrices(const Skeleton& skeleton, const std::vector<XMFLOAT4X4>& pose)
    {
        const size_t jointCount = std::min(skeleton.joints.size(), pose.size());
        std::vector<XMFLOAT4X4> result(jointCount);
        for (size_t i = 0; i < jointCount; ++i)
        {
            // 行ベクトルの形では、左から順に掛ける:頂点 × 逆バインド行列 × 骨の今の行列
            const XMMATRIX inverseBind = XMLoadFloat4x4(&skeleton.joints[i].inverseBind);
            const XMMATRIX world = XMLoadFloat4x4(&pose[i]);
            XMStoreFloat4x4(&result[i], inverseBind * world);
        }
        return result;
    }
}
