#include "KR/Animation/Hitbox.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace KR
{
    std::vector<WorldHitbox> EvaluateHitboxes(const std::vector<Hitbox>& hitboxes, const std::vector<XMFLOAT4X4>& pose,
                                              const XMMATRIX& modelWorld)
    {
        std::vector<WorldHitbox> result(hitboxes.size());
        for (size_t i = 0; i < hitboxes.size(); ++i)
        {
            const Hitbox& hitbox = hitboxes[i];

            // 骨の中の、向きと位置(行ベクトルの形:向き → 位置の順に掛ける)
            XMMATRIX matrix = XMMatrixRotationQuaternion(XMQuaternionNormalize(XMLoadFloat4(&hitbox.rotation)))
                            * XMMatrixTranslation(hitbox.center.x, hitbox.center.y, hitbox.center.z);

            // 骨の今の世界の行列 → モデル全体の行列(範囲の外の骨は、骨の動きなし)
            if (hitbox.jointIndex >= 0 && static_cast<size_t>(hitbox.jointIndex) < pose.size())
            {
                matrix = matrix * XMLoadFloat4x4(&pose[static_cast<size_t>(hitbox.jointIndex)]);
            }
            matrix = matrix * modelWorld;

            // 行列の 1〜3 行目 = 軸(長さは、拡大の倍率)・4 行目 = 位置
            const float scale[3] = {
                XMVectorGetX(XMVector3Length(matrix.r[0])),
                XMVectorGetX(XMVector3Length(matrix.r[1])),
                XMVectorGetX(XMVector3Length(matrix.r[2])),
            };

            WorldHitbox& world = result[i];
            world.shape = hitbox.shape;
            if (hitbox.shape == HitShape::Box)
            {
                XMStoreFloat3(&world.box.center, matrix.r[3]);
                for (int k = 0; k < 3; ++k)
                {
                    // 軸は、長さ 1 に直す(0 に近いときは、そのまま。割り算を避ける)
                    const XMVECTOR axis = scale[k] > 1e-8f ? XMVectorScale(matrix.r[k], 1.0f / scale[k]) : matrix.r[k];
                    XMStoreFloat3(&world.box.axes[k], axis);
                }
                world.box.halfSize = { hitbox.halfSize.x * scale[0], hitbox.halfSize.y * scale[1], hitbox.halfSize.z * scale[2] };
            }
            else
            {
                XMStoreFloat3(&world.sphere.center, matrix.r[3]);
                world.sphere.radius = hitbox.radius * std::max({ scale[0], scale[1], scale[2] });
            }
        }
        return result;
    }

    bool Intersects(const WorldHitbox& a, const WorldHitbox& b)
    {
        if (a.shape == HitShape::Box)
        {
            return b.shape == HitShape::Box ? Intersects(a.box, b.box) : Intersects(a.box, b.sphere);
        }
        return b.shape == HitShape::Box ? Intersects(a.sphere, b.box) : Intersects(a.sphere, b.sphere);
    }

    std::vector<Hitbox> GenerateDefaultHitboxes(const Skeleton& skeleton, float thickness)
    {
        std::vector<Hitbox> hitboxes;
        for (size_t i = 0; i < skeleton.joints.size(); ++i)
        {
            Hitbox hitbox;
            hitbox.name = skeleton.joints[i].name;
            hitbox.jointIndex = static_cast<int>(i);

            // この骨の、最初の子の骨を探す(子の位置は、この骨から見た、位置なので、そのまま、骨の中の座標)
            const Joint* child = nullptr;
            for (const Joint& joint : skeleton.joints)
            {
                if (joint.parent == static_cast<int>(i))
                {
                    child = &joint;
                    break;
                }
            }

            if (child)
            {
                const XMFLOAT3& t = child->translation;
                hitbox.shape = HitShape::Box;
                hitbox.center = { t.x * 0.5f, t.y * 0.5f, t.z * 0.5f };
                hitbox.halfSize = { std::max(std::abs(t.x) * 0.5f, thickness), std::max(std::abs(t.y) * 0.5f, thickness),
                                    std::max(std::abs(t.z) * 0.5f, thickness) };
            }
            else
            {
                hitbox.shape = HitShape::Sphere;
                hitbox.radius = thickness;
            }
            hitboxes.push_back(hitbox);
        }
        return hitboxes;
    }
}
