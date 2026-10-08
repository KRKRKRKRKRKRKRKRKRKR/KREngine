#include "KR/Animation/SkeletonDebug.h"

using namespace DirectX;

namespace KR
{
    std::vector<XMFLOAT4X4> GetBindPoseWorldMatrices(const Skeleton& skeleton)
    {
        std::vector<XMFLOAT4X4> result(skeleton.joints.size());
        for (size_t i = 0; i < skeleton.joints.size(); ++i)
        {
            const XMMATRIX inverseBind = XMLoadFloat4x4(&skeleton.joints[i].inverseBind);
            XMStoreFloat4x4(&result[i], XMMatrixInverse(nullptr, inverseBind));
        }
        return result;
    }

    void DrawSkeleton(Renderer& renderer, const Skeleton& skeleton, const std::vector<XMFLOAT4X4>& jointWorld,
                      const XMMATRIX& world, const XMFLOAT4& color)
    {
        // 行列の 4 行目が、位置(行ベクトルの形)。world を掛けて、モデルの置き場所に動かす
        auto positionOf = [&](size_t index)
        {
            XMFLOAT3 position;
            XMStoreFloat3(&position, XMVector3Transform(XMLoadFloat4x4(&jointWorld[index]).r[3], world));
            return position;
        };

        constexpr float CrossSize = 0.05f; // 根の十字の大きさ(メートル)
        for (size_t i = 0; i < skeleton.joints.size() && i < jointWorld.size(); ++i)
        {
            const XMFLOAT3 position = positionOf(i);
            const int parent = skeleton.joints[i].parent;
            if (parent >= 0)
            {
                renderer.DrawLine(positionOf(static_cast<size_t>(parent)), position, color);
            }
            else
            {
                renderer.DrawLine({ position.x - CrossSize, position.y, position.z }, { position.x + CrossSize, position.y, position.z }, color);
                renderer.DrawLine({ position.x, position.y - CrossSize, position.z }, { position.x, position.y + CrossSize, position.z }, color);
                renderer.DrawLine({ position.x, position.y, position.z - CrossSize }, { position.x, position.y, position.z + CrossSize }, color);
            }
        }
    }
}
