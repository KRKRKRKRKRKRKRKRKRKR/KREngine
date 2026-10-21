#pragma once

#include <DirectXMath.h>

#include <vector>

namespace KR
{
    // 点光源(電球・たいまつ・爆発の光のような、1 点から四方に広がる光)。Lighting::pointLights に入れる
    // 明るさは、距離の 2 乗に反比例して弱くなり、range のところで 0 になる(滑らかに消える)
    struct PointLight
    {
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f }; // 光の位置(世界の座標・メートル)
        DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };    // 光の色(強さ込み。1.0 を超えてもよい。負の値は使えない)
        float range = 10.0f;                               // 光が届く距離(メートル)。これより遠くは、全く照らさない(0 以下の光は、使わない)
    };

    // スポットライト(懐中電灯・車のライトのような、円錐の形に照らす光)。Lighting::spotLights に入れる
    // 明るさは、距離(点光源と同じ)と、円錐の中での位置(中心が一番強く、縁に向かって滑らかに弱くなる)で決まる
    struct SpotLight
    {
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };   // 光の位置(世界の座標・メートル)
        DirectX::XMFLOAT3 direction = { 0.0f, -1.0f, 0.0f }; // 光が向かう向き(長さは Renderer が 1 にそろえる。長さ 0 の光は使わない)
        DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f };      // 光の色(強さ込み)
        float range = 15.0f;                                 // 光が届く距離(メートル)
        float innerAngleDegrees = 15.0f;                     // 内側の角度(度。中心からの角度)。この内側は、一番明るい
        float outerAngleDegrees = 30.0f;                     // 外側の角度(度)。ここで 0 になる。内側より小さいときは、入れ替える(警告)
    };

    // 光の数の上限(超えた分は、警告を 1 回出して、使わない。シェーダーの配列の大きさと同じ)
    constexpr size_t MaxPointLights = 16;
    constexpr size_t MaxSpotLights = 8;

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

        // 点光源・スポットライト(太陽とは別に、いくつでも置ける。上限は MaxPointLights / MaxSpotLights)。空なら、今までと同じ見た目
        std::vector<PointLight> pointLights;
        std::vector<SpotLight> spotLights;
    };

    // 空(地平線が明るく、真上が暗いグラデーション)の設定。Game が持ち、毎フレーム Renderer::SetSky で渡す
    // 色は「物理的な明るさ」の値(ガンマ補正の前)。フォグの色は、地平線の色に自動で合わせる(遠くが空に溶けるように)
    struct Sky
    {
        DirectX::XMFLOAT3 horizonColor = { 0.85f, 0.55f, 0.50f }; // 地平線(視線が水平)の色。フォグの色も、これに合わせる
        DirectX::XMFLOAT3 zenithColor = { 0.15f, 0.15f, 0.40f };  // 天頂(真上)の色
        DirectX::XMFLOAT3 nadirColor = { 0.47f, 0.30f, 0.28f };   // 下の色(視線が真下に近いとき。地面が描かれない所に見える)
        float horizonWidth = 0.45f; // 地平線の光の幅(視線の高さの単位。0 〜 1)。小さいほど細い帯、大きいほど天頂の近くまで地平線の色が広がる
    };

    // フォグ(遠くが霧の色に溶けて見える)の設定。Game が持ち、毎フレーム Renderer::SetFog で渡す
    // 霧の色は、空の地平線の色(Sky::horizonColor)に自動で合わせる(遠くが空に溶けるように)
    struct Fog
    {
        bool enabled = true;
        float density = 0.02f; // 濃さ。見える割合 = exp(-濃さ × 距離)。大きいほど、近くから霞む
    };
}
