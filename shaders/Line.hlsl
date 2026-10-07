// 線: デバッグ用の線(当たり判定の箱など)を、決めた色のまま描く。光・フォグの計算はしない(色がくすまないように)
// b1 の別紙(カメラの行列)は、Basic.hlsl と共通。頂点は、すでに世界の座標なので、ワールド行列(b0)は使わない
#include "Common.hlsli"

// 頂点シェーダーへの入力。C++ 側の入力レイアウト(Renderer.cpp の LineInputLayout)と名前・順番を合わせる
struct VSInput
{
    float3 position : POSITION; // 世界の座標(メートル)
    float4 color    : COLOR;
};

struct PSInput
{
    float4 position : SV_Position;
    float4 color    : COLOR;
};

// 頂点シェーダー: 世界の座標を、カメラの行列で、画面の位置にする
PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0), viewProjection);
    output.color = input.color;
    return output;
}

// ピクセルシェーダー: 頂点の色を、そのまま塗る
float4 PSMain(PSInput input) : SV_Target
{
    return input.color;
}
