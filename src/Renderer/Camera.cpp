#include "KR/Renderer/Camera.h"

using namespace DirectX;

namespace KR
{
    XMMATRIX Camera::GetViewMatrix() const
    {
        // XMFLOAT3(保存用)を XMVECTOR(計算用)に読み込んでから計算する
        // LH = Left-Handed(左手座標系)
        const XMVECTOR position = XMLoadFloat3(&m_Position);
        const XMVECTOR target = XMLoadFloat3(&m_Target);
        XMVECTOR up = XMLoadFloat3(&m_Up);

        // ロール: 「上」の向きを、視線の向きを軸に、回す(カメラが、視線を軸に傾く)
        if (m_RollDegrees != 0.0f)
        {
            const XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(target, position));
            up = XMVector3TransformNormal(up, XMMatrixRotationAxis(forward, XMConvertToRadians(m_RollDegrees)));
        }

        return XMMatrixLookAtLH(position, target, up);
    }

    XMMATRIX Camera::GetProjectionMatrix(float aspectRatio) const
    {
        return XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FieldOfViewDegrees), aspectRatio, m_NearZ, m_FarZ);
    }

    Ray Camera::ScreenPointToRay(float x, float y, float width, float height) const
    {
        // 画面の点を、奥行き 0(いちばん手前 = near)と、奥行き 1(いちばん奥 = far)の 2 点で、世界の座標に戻す(Unproject)
        // その 2 点を結んだ向きが、光線の向き
        const XMMATRIX projection = GetProjectionMatrix(width / height);
        const XMMATRIX view = GetViewMatrix();
        const XMVECTOR nearPoint = XMVector3Unproject(XMVectorSet(x, y, 0.0f, 1.0f), 0.0f, 0.0f, width, height, 0.0f, 1.0f, projection, view, XMMatrixIdentity());
        const XMVECTOR farPoint = XMVector3Unproject(XMVectorSet(x, y, 1.0f, 1.0f), 0.0f, 0.0f, width, height, 0.0f, 1.0f, projection, view, XMMatrixIdentity());

        Ray ray;
        XMStoreFloat3(&ray.origin, nearPoint);
        XMStoreFloat3(&ray.direction, XMVector3Normalize(XMVectorSubtract(farPoint, nearPoint)));
        return ray;
    }
}
