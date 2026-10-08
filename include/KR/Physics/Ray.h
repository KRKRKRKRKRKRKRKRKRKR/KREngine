#pragma once

#include <DirectXMath.h>

namespace KR
{
    // 光線(レイ):始点から、ある向きに、まっすぐ伸びる線。画面の点(マウスの位置)から、3D の何に触れたかを調べるのに使う
    struct Ray
    {
        DirectX::XMFLOAT3 origin = { 0.0f, 0.0f, 0.0f };    // 始点(世界の座標。メートル)
        DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f }; // 向き(長さ 1)
    };
}
