#include "KR/Animation/HitboxMirror.h"

#include <cmath>

using namespace DirectX;

namespace KR
{
    Hitbox MirrorHitbox(const Hitbox& source, const XMFLOAT4X4& sourceBoneWorld, int targetJoint, const XMFLOAT4X4& targetBoneWorld)
    {
        // ① 形 → 元の骨の中の行列 → モデルの座標(行ベクトル:向き → 位置 → 骨 の順に掛ける)
        const XMMATRIX local = XMMatrixRotationQuaternion(XMQuaternionNormalize(XMLoadFloat4(&source.rotation)))
                             * XMMatrixTranslation(source.center.x, source.center.y, source.center.z);
        const XMMATRIX inModel = local * XMLoadFloat4x4(&sourceBoneWorld);

        // ② X を反転(行列の右から、X を −1 倍する)→ ③ 対の骨の中の座標に戻す
        const XMMATRIX mirrored = inModel * XMMatrixScaling(-1.0f, 1.0f, 1.0f);
        XMMATRIX inTarget = mirrored * XMMatrixInverse(nullptr, XMLoadFloat4x4(&targetBoneWorld));

        Hitbox result = source;
        result.jointIndex = targetJoint;
        XMStoreFloat3(&result.center, inTarget.r[3]);

        if (source.shape == HitShape::Box)
        {
            // 1〜3 行目 = 箱の軸(長さは、骨の拡大の差。1 なら、そのまま)
            float length[3];
            for (int k = 0; k < 3; ++k)
            {
                length[k] = XMVectorGetX(XMVector3Length(inTarget.r[k]));
                if (length[k] > 1e-8f)
                {
                    inTarget.r[k] = XMVectorScale(inTarget.r[k], 1.0f / length[k]);
                }
                inTarget.r[k] = XMVectorSetW(inTarget.r[k], 0.0f);
            }
            inTarget.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

            // 反転で、座標の右手・左手が入れ替わった(3 本の軸の行列式が、負)→ X の軸の向きを逆にして、普通の回転にする
            if (XMVectorGetX(XMMatrixDeterminant(inTarget)) < 0.0f)
            {
                inTarget.r[0] = XMVectorNegate(inTarget.r[0]);
                inTarget.r[0] = XMVectorSetW(inTarget.r[0], 0.0f);
            }
            XMStoreFloat4(&result.rotation, XMQuaternionNormalize(XMQuaternionRotationMatrix(inTarget)));
            result.halfSize = { source.halfSize.x * length[0], source.halfSize.y * length[1], source.halfSize.z * length[2] };
        }
        return result;
    }
}
