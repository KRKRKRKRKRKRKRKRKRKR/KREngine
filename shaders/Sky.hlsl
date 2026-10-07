// 空: 画面全体に、地平線が明るく、真上が暗いグラデーションを描く。最初に描き、あとから地面や物が上書きする
// b1 の別紙(カメラ・空の色など)は、Basic.hlsl と共通
#include "Common.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float2 ndc      : TEXCOORD0; // この画素の、画面上の座標(x, y とも -1 〜 1)
};

// 頂点シェーダー: 頂点の番号(0, 1, 2)から、画面全体を覆う大きな三角形の位置を作る(頂点バッファは要らない)
//   番号 0 → (-1, -1) / 番号 1 → (3, -1) / 番号 2 → (-1, 3)  ← 画面(-1〜1 の四角)全体を含む三角形
VSOutput VSMain(uint vertexId : SV_VertexID)
{
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2); // (0,0) (2,0) (0,2)
    float2 ndc = uv * 2.0 - 1.0;

    VSOutput output;
    output.position = float4(ndc, 1.0, 1.0); // 深度は使わない(空のパイプラインは、深度テストなし)
    output.ndc = ndc;
    return output;
}

// ピクセルシェーダー: この画素を通る「カメラからの視線の向き」を求めて、その高さで空の色を決める
float4 PSMain(VSOutput input) : SV_Target
{
    // 画面上の座標(x, y)と、一番奥(z = 1)の点を、カメラの行列の逆で戻す
    // inverseViewProjection は、カメラの位置を含まない(向きだけ)ので、戻した点は、カメラからの相対位置 = 視線の向き。
    // カメラの位置を引かない(位置を足し引きすると、自機が遠くへ行くほど、誤差が増えて、地平線がちらつく)
    float4 farPoint = mul(float4(input.ndc, 1.0, 1.0), inverseViewProjection);
    float3 viewDirection = normalize(farPoint.xyz / farPoint.w);

    // 視線の高さ s: 水平なら 0、真上なら 1、真下なら -1
    float s = viewDirection.y;

    // 地平線の光の強さ w: 地平線(s = 0)で 1、離れるほど 0 に近づく釣り鐘型。地平線で、グラフが水平(傾き 0)なので、
    // 地平線をまたいでも、色がなめらかにつながる(折れ目の線が出ない)。幅(skyZenith.w)が小さいほど、細い帯になる
    float width = max(skyZenith.w, 0.01);
    float w = exp(-(s * s) / (width * width));

    // 地平線から遠い所の色: 上向きなら天頂の色、下向きなら下の色
    float3 farColor = (s >= 0.0) ? skyZenith.rgb : skyNadir.rgb;

    float3 skyColor = lerp(farColor, skyHorizon.rgb, w);
    return float4(skyColor, 1.0);
}
