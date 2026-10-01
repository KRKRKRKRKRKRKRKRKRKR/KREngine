#pragma once

#include <DirectXMath.h>

namespace TenFiftyEight
{
    // 光の設定。Game が持ち、毎フレーム Renderer::SetLighting で渡す(SetCamera と同じ形)
    // 太陽のような「平行な光」(世界のどこでも同じ向き)と、環境光(光が当たらない面の最低限の明るさ)
    struct Lighting
    {
        // 物から太陽へ向かう向き(長さは Renderer が 1 にそろえる)。面がこちらを向くほど明るい
        DirectX::XMFLOAT3 toLight = { -0.45f, 0.70f, -0.55f };

        // 太陽の光の色(強さ込み。1.0 を超えてもよい)。色は「物理的な明るさ」の値(ガンマ補正の前)
        DirectX::XMFLOAT3 color = { 1.00f, 0.95f, 0.85f };

        // 環境光。光が当たらない面も、これだけは明るい(空からの照り返しのようなもの)
        DirectX::XMFLOAT3 ambient = { 0.30f, 0.33f, 0.40f };
    };
}
