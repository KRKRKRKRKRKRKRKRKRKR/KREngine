// 複数のシェーダー(Basic.hlsl・Sky.hlsl など)が共有する宣言。#include "Common.hlsli" で取り込む
// 「C++ 側の FrameConstants(Renderer.cpp)と対応する宣言」を、1か所だけに書くためのファイル
// (別々のファイルに書くと、片方だけ直し忘れて食い違い、読む値がずれて絵が狂う)

// b1 の別紙(定数バッファ)から受け取るデータ(C++ 側は Renderer::BeginFrame で1フレームに1回書く)
// 頂点シェーダーもピクセルシェーダーも読む(Root Signature の b1 は ALL)
// 並びと大きさは、C++ 側の FrameConstants と完全に同じにする
cbuffer FrameConstants : register(b1)
{
    row_major float4x4 viewProjection; // ビュー行列 × プロジェクション行列(カメラ)
    float4 toLight;                    // xyz = 物から太陽へ向かう向き(長さ 1。平行な光なので、世界のどこでも同じ)
    float4 lightColor;                 // rgb = 太陽の光の色(強さ込み)
    float4 ambientColor;               // rgb = 環境光(光が当たらない面の、最低限の明るさ)
    float4 cameraPosition;             // xyz = カメラの世界での位置(フォグの距離の計算に使う)
    float4 fogColor;                   // rgb = 霧の色(空の地平線の色と同じ。遠くが空に溶ける)
    float4 fogParams;                  // x = フォグの濃さ(0 ならフォグなし)
    row_major float4x4 inverseViewProjection; // viewProjection の逆(空: 画面の座標 → カメラからの向き。カメラの位置は含まない)
    float4 skyHorizon;                 // rgb = 地平線の色
    float4 skyZenith;                  // rgb = 天頂の色 / w = 地平線の光の幅
    float4 skyNadir;                   // rgb = 下の色(視線が地平線より下のとき)
    float4 lightCounts;                // x = 点光源の数 / y = スポットライトの数
    float4 pointPositionRange[16];     // xyz = 位置 / w = 届く距離(16 は、C++ 側の MaxPointLights)
    float4 pointColor[16];             // rgb = 色(強さ込み)
    float4 spotPositionRange[8];       // xyz = 位置 / w = 届く距離(8 は、C++ 側の MaxSpotLights)
    float4 spotDirectionOuter[8];      // xyz = 向き(長さ 1。光が向かう向き) / w = cos(外側の角度)
    float4 spotColorInner[8];          // rgb = 色(強さ込み) / w = cos(内側の角度)
};

// ハイライト(Blinn-Phong): 光と視線の「真ん中の向き」(ハーフベクトル)が、面の向きに近いほど強い
// N = 面の向き / L = 物から光へ向かう向き / V = 物からカメラへ向かう向き(どれも長さ 1)/ shininess = 鋭さ(大きいほど小さく鋭い点)
// 光が当たらない面(N・L が 0 以下)には出さない(面の裏側に、点が出るのを防ぐ)
float BlinnPhong(float3 N, float3 L, float3 V, float shininess)
{
    float3 H = normalize(L + V);
    float highlight = pow(saturate(dot(N, H)), max(shininess, 1.0));
    return dot(N, L) > 0.0 ? highlight : 0.0;
}

// 光の距離による弱まり: 距離の 2 乗に反比例(現実の光と同じ)× 届く距離(range)のところで 0 になる窓(滑らかに消える)
// 光の色は「1 メートルの距離での明るさ」。距離 0 でも割り算が壊れないよう、分母に小さい数を足す
float Attenuation(float distanceToLight, float range)
{
    float ratio = distanceToLight / range;
    float window = saturate(1.0 - ratio * ratio * ratio * ratio); // 1 - (d/range)^4
    return window * window / (distanceToLight * distanceToLight + 0.01);
}

// スポットライトの円錐の中での強さ(0 〜 1): 内側の角度の内側は 1・外側の角度の外側は 0・その間は滑らかにつなぐ
// directionToLight = 物から光へ向かう向き / spotDirection = 光が向かう向き(どちらも長さ 1)。cos の大きい方が、角度が小さい
float SpotFactor(float3 directionToLight, float3 spotDirection, float cosOuter, float cosInner)
{
    float cosAngle = dot(-directionToLight, spotDirection);
    float t = saturate((cosAngle - cosOuter) / max(cosInner - cosOuter, 0.0001));
    return t * t * (3.0 - 2.0 * t);
}

// トゥーン: 0 〜 1 の明るさ x を、steps 段階(等間隔の steps 個の値)に切る。steps が 2 未満なら、x をそのまま返す(なめらか)
// softness = 帯の境目のなめらかさ(0 〜 0.5。帯 1 つの幅に対する割合)。0 に近いほど、くっきり
float Toon(float x, float steps, float softness)
{
    if (steps < 2.0)
    {
        return x;
    }
    float y = saturate(x) * steps;
    float band = floor(y);
    float edge = smoothstep(0.0, max(softness, 0.0001), y - band);   // 帯の下の縁で、1 つ下の帯から、この帯へ、つなぐ
    return saturate((band - 1.0 + edge) / (steps - 1.0));              // 最低の帯 = 0・最高の帯 = 1(x = 1 のとき)
}

// トゥーンのハイライト: なめらかな ハイライトの強さ h(0 〜 1)を、しきい値 0.5 で、はっきりした点にする(steps が 2 未満なら h のまま)
float ToonSpecular(float h, float steps, float softness)
{
    if (steps < 2.0)
    {
        return h;
    }
    float width = max(softness * 0.5, 0.0001);
    return smoothstep(0.5 - width, 0.5 + width, h);
}
