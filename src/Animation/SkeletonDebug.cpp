#include "KR/Animation/SkeletonDebug.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace DirectX;

namespace KR
{
    namespace
    {
        // 骨の形(八面体)の寸法。長さを 1 とした割合(Blender の骨と同じ。いちばん太いのは、根もとから 10% のところで、幅は 10%)
        constexpr float WidestAt = 0.1f;
        constexpr float HalfWidth = 0.1f;

        // 八面体の 6 つの点:根もと・先・太い輪の 4 点。形は、根もとが原点・先が +Y の向き
        struct BoneShape
        {
            XMFLOAT3 head = { 0.0f, 0.0f, 0.0f };
            XMFLOAT3 tail = { 0.0f, 1.0f, 0.0f };
            XMFLOAT3 ring[4] = {
                { HalfWidth, WidestAt, 0.0f },
                { 0.0f, WidestAt, HalfWidth },
                { -HalfWidth, WidestAt, 0.0f },
                { 0.0f, WidestAt, -HalfWidth },
            };
        };

        // 八面体の形のデータ。面ごとに、頂点を分けて、法線をそろえる(角ばった陰影になる)
        MeshData CreateBoneMeshData(const XMFLOAT4& color)
        {
            const BoneShape shape;
            const XMVECTOR inside = XMVectorSet(0.0f, WidestAt, 0.0f, 0.0f); // 形の内側の点(輪の中心)。面の向き(表)を決める目印

            MeshData data;
            auto addTriangle = [&](XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c)
            {
                const XMVECTOR va = XMLoadFloat3(&a);
                const XMVECTOR vb = XMLoadFloat3(&b);
                const XMVECTOR vc = XMLoadFloat3(&c);
                XMVECTOR normal = XMVector3Cross(XMVectorSubtract(vb, va), XMVectorSubtract(vc, va));

                // このエンジンは「時計回りが表」(左手系)。外積の向きが、形の外に向いていれば、今の頂点の並びが、表。内向きなら、2 番目と 3 番目を入れ替える
                const XMVECTOR centroid = XMVectorScale(XMVectorAdd(XMVectorAdd(va, vb), vc), 1.0f / 3.0f);
                if (XMVectorGetX(XMVector3Dot(normal, XMVectorSubtract(centroid, inside))) < 0.0f)
                {
                    std::swap(b, c);
                    normal = XMVectorNegate(normal);
                }

                XMFLOAT3 n;
                XMStoreFloat3(&n, XMVector3Normalize(normal));
                const uint32_t base = static_cast<uint32_t>(data.vertices.size());
                for (const XMFLOAT3& p : { a, b, c })
                {
                    Vertex vertex;
                    vertex.position = p;
                    vertex.normal = n;
                    vertex.color = color;
                    data.vertices.push_back(vertex);
                }
                data.indices.push_back(base);
                data.indices.push_back(base + 1);
                data.indices.push_back(base + 2);
            };

            for (int i = 0; i < 4; ++i)
            {
                const XMFLOAT3& current = shape.ring[i];
                const XMFLOAT3& next = shape.ring[(i + 1) % 4];
                addTriangle(shape.head, current, next); // 根もと側の 4 面
                addTriangle(shape.tail, current, next); // 先側の 4 面
            }
            return data;
        }

        // 向き(長さ 1 の矢印)y を +Y に合わせる回転の行列を作る。行 = 回したあとの、右・上・前の軸。長さ scale を掛ける(拡大も兼ねる)
        XMMATRIX MakeBoneMatrix(FXMVECTOR head, FXMVECTOR direction, float length)
        {
            const XMVECTOR y = direction;
            // y に平行でない、適当な向きを選んで、外積で、y に垂直な x を作る(骨の回転 = ロールは、決まらないので、適当)
            const XMVECTOR reference = (std::abs(XMVectorGetY(y)) < 0.99f) ? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
            const XMVECTOR x = XMVector3Normalize(XMVector3Cross(reference, y));
            const XMVECTOR z = XMVector3Cross(x, y); // x × y → 回転(鏡映しにならない)
            XMMATRIX matrix;
            matrix.r[0] = XMVectorScale(x, length);
            matrix.r[1] = XMVectorScale(y, length);
            matrix.r[2] = XMVectorScale(z, length);
            matrix.r[3] = XMVectorSetW(head, 1.0f);
            return matrix;
        }
    }

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

    void BoneRenderer::Create(Renderer& renderer, const XMFLOAT4& color, const XMFLOAT4& selectedColor)
    {
        m_Normal = renderer.CreateMesh(CreateBoneMeshData(color));
        m_Selected = renderer.CreateMesh(CreateBoneMeshData(selectedColor));
    }

    void BoneRenderer::Destroy(Renderer& renderer)
    {
        if (m_Normal.IsValid())
        {
            renderer.DestroyMesh(m_Normal);
            m_Normal = {};
        }
        if (m_Selected.IsValid())
        {
            renderer.DestroyMesh(m_Selected);
            m_Selected = {};
        }
    }

    void BoneRenderer::Draw(Renderer& renderer, const Skeleton& skeleton, const std::vector<XMFLOAT4X4>& jointWorld,
                            const XMMATRIX& world, const Style& style) const
    {
        const size_t jointCount = std::min(skeleton.joints.size(), jointWorld.size());

        // 各関節の位置(行列の 4 行目)と、子を持つか
        std::vector<XMVECTOR> position(jointCount);
        std::vector<bool> hasChild(jointCount, false);
        for (size_t i = 0; i < jointCount; ++i)
        {
            position[i] = XMLoadFloat4x4(&jointWorld[i]).r[3];
            const int parent = skeleton.joints[i].parent;
            if (parent >= 0 && static_cast<size_t>(parent) < jointCount)
            {
                hasChild[static_cast<size_t>(parent)] = true;
            }
        }

        const BoneShape shape;
        // 骨 1 本を描く:塗りつぶした形 + 縁の線
        auto drawBone = [&](size_t headJoint, FXMVECTOR head, FXMVECTOR tail)
        {
            const XMVECTOR axis = XMVectorSubtract(tail, head);
            const float length = XMVectorGetX(XMVector3Length(axis));
            if (length < 1e-6f)
            {
                return;
            }

            const XMMATRIX matrix = MakeBoneMatrix(head, XMVectorScale(axis, 1.0f / length), length) * world;
            const bool selected = static_cast<int>(headJoint) == style.selectedJoint;
            renderer.DrawMesh(selected ? m_Selected : m_Normal, matrix, {}, style.inFront);

            // 縁の線:根もと → 輪の 4 点・輪の 4 点 → 先・輪の 4 辺(八面体の 12 本の辺)
            XMFLOAT3 corner[6];
            XMStoreFloat3(&corner[0], XMVector3Transform(XMLoadFloat3(&shape.head), matrix));
            XMStoreFloat3(&corner[1], XMVector3Transform(XMLoadFloat3(&shape.tail), matrix));
            for (int k = 0; k < 4; ++k)
            {
                XMStoreFloat3(&corner[2 + k], XMVector3Transform(XMLoadFloat3(&shape.ring[k]), matrix));
            }
            for (int k = 0; k < 4; ++k)
            {
                renderer.DrawLine(corner[0], corner[2 + k], style.outlineColor, style.inFront);
                renderer.DrawLine(corner[2 + k], corner[1], style.outlineColor, style.inFront);
                renderer.DrawLine(corner[2 + k], corner[2 + (k + 1) % 4], style.outlineColor, style.inFront);
            }
        };

        for (size_t i = 0; i < jointCount; ++i)
        {
            const int parent = skeleton.joints[i].parent;
            if (parent < 0 || static_cast<size_t>(parent) >= jointCount)
            {
                continue; // 根:自分は、子の関節から見たときに、骨の根もとになる(親から伸びる骨は、無い)
            }

            // 親の関節から、この関節まで(親の骨の、1 本)
            drawBone(static_cast<size_t>(parent), position[static_cast<size_t>(parent)], position[i]);

            // この関節が先端(子が無い)なら、親から来た向きの先に、短い骨(親の骨の 3 割の長さ)
            if (!hasChild[i])
            {
                const XMVECTOR direction = XMVectorSubtract(position[i], position[static_cast<size_t>(parent)]);
                drawBone(i, position[i], XMVectorAdd(position[i], XMVectorScale(direction, 0.3f)));
            }
        }
    }
}
