#include "KR/Physics/Collision.h"

#include <cmath>

using namespace DirectX;

namespace KR
{
    namespace
    {
        // 外積の長さ(の 2 乗)が、これより小さければ、2 本の軸が、ほぼ平行とみなして、その軸を飛ばす
        // (平行な軸の外積は、ほぼ 0 の矢印になり、向きを持たない。回転しない箱どうしでは、必ず起きる)
        constexpr float ParallelEpsilonSq = 1e-6f * 1e-6f;

        // 箱を、軸 axis に投影した影の「半分の長さ」(箱の 3 本の軸を、それぞれ axis に投影して、半分の長さを掛けて足す)
        float ProjectedRadius(const OBB& box, FXMVECTOR axis)
        {
            float radius = 0.0f;
            const float half[3] = { box.halfSize.x, box.halfSize.y, box.halfSize.z };
            for (int i = 0; i < 3; ++i)
            {
                const float dot = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&box.axes[i]), axis));
                radius += std::abs(dot) * half[i];
            }
            return radius;
        }

        // 軸 axis で、2 つの箱の影にすき間があるか(= この軸が「分離軸」か)
        // axis は、長さ 1 でなくてよい(影の長さも、中心の距離も、同じ倍率で伸び縮みするので、比べた結果は変わらない)
        bool IsSeparated(const OBB& a, const OBB& b, FXMVECTOR axis, FXMVECTOR centerOffset)
        {
            const float distance = std::abs(XMVectorGetX(XMVector3Dot(centerOffset, axis))); // 中心どうしの距離の影
            return distance > ProjectedRadius(a, axis) + ProjectedRadius(b, axis);
        }
    }

    OBB MakeOBB(const XMFLOAT3& center, const XMFLOAT3& halfSize, FXMMATRIX rotation)
    {
        OBB box;
        box.center = center;
        box.halfSize = halfSize;
        // 行列の 1〜3 行目 = 回したあとの右・上・前の向き
        XMFLOAT3X3 rows;
        XMStoreFloat3x3(&rows, rotation);
        box.axes[0] = { rows._11, rows._12, rows._13 };
        box.axes[1] = { rows._21, rows._22, rows._23 };
        box.axes[2] = { rows._31, rows._32, rows._33 };
        return box;
    }

    bool Intersects(const OBB& a, const OBB& b)
    {
        const XMVECTOR centerOffset = XMVectorSubtract(XMLoadFloat3(&b.center), XMLoadFloat3(&a.center));

        XMVECTOR axesA[3];
        XMVECTOR axesB[3];
        for (int i = 0; i < 3; ++i)
        {
            axesA[i] = XMLoadFloat3(&a.axes[i]);
            axesB[i] = XMLoadFloat3(&b.axes[i]);
        }

        // ① A の 3 軸・② B の 3 軸(面の向き)。1 本でもすき間があれば、当たっていない
        for (int i = 0; i < 3; ++i)
        {
            if (IsSeparated(a, b, axesA[i], centerOffset) || IsSeparated(a, b, axesB[i], centerOffset))
            {
                return false;
            }
        }

        // ③ A の軸 × B の軸 の 9 本(辺と辺が、すれ違う向き)。平行な組み合わせは飛ばす
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                const XMVECTOR axis = XMVector3Cross(axesA[i], axesB[j]);
                if (XMVectorGetX(XMVector3LengthSq(axis)) < ParallelEpsilonSq)
                {
                    continue;
                }
                if (IsSeparated(a, b, axis, centerOffset))
                {
                    return false;
                }
            }
        }

        // 15 本の、どの軸でも、影が重なっていた → 当たっている
        return true;
    }

    std::array<XMFLOAT3, 8> GetCorners(const OBB& box)
    {
        const XMVECTOR center = XMLoadFloat3(&box.center);
        const XMVECTOR right = XMVectorScale(XMLoadFloat3(&box.axes[0]), box.halfSize.x);
        const XMVECTOR up = XMVectorScale(XMLoadFloat3(&box.axes[1]), box.halfSize.y);
        const XMVECTOR forward = XMVectorScale(XMLoadFloat3(&box.axes[2]), box.halfSize.z);

        std::array<XMFLOAT3, 8> corners;
        for (int i = 0; i < 8; ++i)
        {
            // ビット 0 / 1 / 2 が 1 なら +、0 なら −
            XMVECTOR corner = center;
            corner = XMVectorAdd(corner, (i & 1) ? right : XMVectorNegate(right));
            corner = XMVectorAdd(corner, (i & 2) ? up : XMVectorNegate(up));
            corner = XMVectorAdd(corner, (i & 4) ? forward : XMVectorNegate(forward));
            XMStoreFloat3(&corners[i], corner);
        }
        return corners;
    }
}
