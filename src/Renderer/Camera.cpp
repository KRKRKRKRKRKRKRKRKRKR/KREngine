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
}
