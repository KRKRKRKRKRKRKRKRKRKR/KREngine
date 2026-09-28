#include "TenFiftyEight/Renderer/Camera.h"

using namespace DirectX;

namespace TenFiftyEight
{
    XMMATRIX Camera::GetViewMatrix() const
    {
        // XMFLOAT3(保存用)を XMVECTOR(計算用)に読み込んでから計算する
        // LH = Left-Handed(左手座標系)
        return XMMatrixLookAtLH(XMLoadFloat3(&m_Position), XMLoadFloat3(&m_Target), XMLoadFloat3(&m_Up));
    }

    XMMATRIX Camera::GetProjectionMatrix(float aspectRatio) const
    {
        return XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FieldOfViewDegrees), aspectRatio, m_NearZ, m_FarZ);
    }
}
