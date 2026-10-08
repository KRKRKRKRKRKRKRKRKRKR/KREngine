#include "KR/Physics/Collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

    bool Intersects(const Sphere& a, const Sphere& b)
    {
        // 距離を 2 乗のまま比べる(平方根を避ける)。半径の和も 2 乗にする
        const XMVECTOR offset = XMVectorSubtract(XMLoadFloat3(&b.center), XMLoadFloat3(&a.center));
        const float distanceSq = XMVectorGetX(XMVector3LengthSq(offset));
        const float radiusSum = a.radius + b.radius;
        return distanceSq <= radiusSum * radiusSum;
    }

    bool Intersects(const Sphere& sphere, const OBB& box)
    {
        // 球の中心を、箱の軸ごとに測り(箱の中心からの距離)、箱の範囲(± 半分の大きさ)に収める → 箱の上で、一番近い点
        const XMVECTOR offset = XMVectorSubtract(XMLoadFloat3(&sphere.center), XMLoadFloat3(&box.center));
        const float half[3] = { box.halfSize.x, box.halfSize.y, box.halfSize.z };

        XMVECTOR closest = XMLoadFloat3(&box.center);
        for (int i = 0; i < 3; ++i)
        {
            const XMVECTOR axis = XMLoadFloat3(&box.axes[i]);
            const float distanceAlongAxis = XMVectorGetX(XMVector3Dot(offset, axis));
            const float clamped = std::clamp(distanceAlongAxis, -half[i], half[i]);
            closest = XMVectorAdd(closest, XMVectorScale(axis, clamped));
        }

        // 一番近い点までの距離(2 乗)と、半径(2 乗)を比べる。球の中心が箱の中なら、距離は 0 で、当たり
        const float distanceSq = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(XMLoadFloat3(&sphere.center), closest)));
        return distanceSq <= sphere.radius * sphere.radius;
    }

    bool Intersects(const OBB& box, const Sphere& sphere)
    {
        return Intersects(sphere, box);
    }

    std::optional<float> Raycast(const Ray& ray, const Sphere& sphere)
    {
        // |origin + t * dir - center|² = r² を t について解く:t² + 2bt + c = 0(dir の長さは 1)
        const XMVECTOR toOrigin = XMVectorSubtract(XMLoadFloat3(&ray.origin), XMLoadFloat3(&sphere.center));
        const float b = XMVectorGetX(XMVector3Dot(toOrigin, XMLoadFloat3(&ray.direction)));
        const float c = XMVectorGetX(XMVector3LengthSq(toOrigin)) - sphere.radius * sphere.radius;

        if (c <= 0.0f)
        {
            return 0.0f; // 始点が球の中
        }
        if (b > 0.0f)
        {
            return std::nullopt; // 球は後ろ(外にいて、遠ざかっている)
        }
        const float discriminant = b * b - c;
        if (discriminant < 0.0f)
        {
            return std::nullopt; // かすりもしない
        }
        return -b - std::sqrt(discriminant); // 手前の交点
    }

    std::optional<float> Raycast(const Ray& ray, const OBB& box)
    {
        const XMVECTOR toCenter = XMVectorSubtract(XMLoadFloat3(&box.center), XMLoadFloat3(&ray.origin));
        const XMVECTOR direction = XMLoadFloat3(&ray.direction);
        const float half[3] = { box.halfSize.x, box.halfSize.y, box.halfSize.z };

        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();
        for (int i = 0; i < 3; ++i)
        {
            const XMVECTOR axis = XMLoadFloat3(&box.axes[i]);
            const float distance = XMVectorGetX(XMVector3Dot(toCenter, axis));   // 始点から見た、箱の中心の、軸方向の位置
            const float speed = XMVectorGetX(XMVector3Dot(direction, axis));     // レイが、軸方向に進む速さ

            if (std::abs(speed) < 1e-8f)
            {
                // 軸と直角に進む:始点が 2 枚の面の間に無ければ、永久に入らない
                if (std::abs(distance) > half[i])
                {
                    return std::nullopt;
                }
                continue;
            }
            float t1 = (distance - half[i]) / speed;
            float t2 = (distance + half[i]) / speed;
            if (t1 > t2)
            {
                std::swap(t1, t2);
            }
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax)
            {
                return std::nullopt;
            }
        }
        return tMin;
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
