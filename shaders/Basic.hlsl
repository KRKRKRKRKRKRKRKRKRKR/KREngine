// b0 の箱から受け取るデータ(C++ 側は SetGraphicsRoot32BitConstants で入れる)
cbuffer ObjectConstants : register(b0)
{
    // row_major: DirectXMath と同じく、行列を「横に読む」
    row_major float4x4 world; // ワールド行列(物体の拡大・回転・移動)
};

// b1 の別紙(フレームごとのデータ。カメラ・光・フォグ)の宣言は、Sky.hlsl と共有するので、共通のファイルにある
#include "Common.hlsli"

// 貼る画像と、その読み方(C++ 側は Root Signature の t0 の口と、静的サンプラー s0)
// 画像を貼らない物には、白の 1×1 が入っている(色 × 白 = 色 なので、今までと同じ色になる)
Texture2D albedoTexture : register(t0);
SamplerState linearSampler : register(s0);

// 頂点シェーダーへの入力。C++ 側の入力レイアウト(Renderer.cpp)と名前・順番を合わせる
struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL; // 面が向いている方向(陰影の計算に使う)
    float4 color    : COLOR;
    float2 uv       : TEXCOORD; // 画像の、どこの色を使うか(0 〜 1)。6-4 で、画像から色を取るのに使う
};

// 頂点シェーダーからピクセルシェーダーへ渡すデータ
struct PSInput
{
    float4 position      : SV_Position; // 画面上の位置(GPU が三角形を画素に分解するのに使う)
    float3 worldPosition : TEXCOORD0;   // 世界での位置(将来、シャドウマップやフォグの距離に使う)
    float3 worldNormal   : TEXCOORD1;   // 世界での法線(三角形の内側では、3頂点の法線が自動で補間される)
    float4 color         : COLOR;       // 三角形の内側では、3頂点の色が自動で補間される
    float2 uv            : TEXCOORD2;   // 三角形の内側では、3頂点の uv が自動で補間される
};

// 頂点シェーダー: 頂点1つにつき1回呼ばれ、「画面のどこに置くか」を決める
PSInput VSMain(VSInput input)
{
    PSInput output;
    // w = 1(位置)にしてから行列を掛ける → 拡大・回転・移動がすべて効く
    float4 worldPosition = mul(float4(input.position, 1.0), world); // ① ローカル → ワールド
    output.position = mul(worldPosition, viewProjection);           // ② ワールド → カメラ → 画面
    // この後 GPU が自動で x, y を w(奥行き)で割る → 遠い物ほど小さく映る
    output.worldPosition = worldPosition.xyz;

    // 法線は「向き」なので、w = 0 にして行列を掛ける → 回転と拡大だけが効き、移動は効かない
    // (長さは変わることがあるので、ピクセルシェーダーで 1 にそろえ直す)
    output.worldNormal = mul(float4(input.normal, 0.0), world).xyz;

    output.color = input.color;
    output.uv = input.uv;
    return output;
}

// ピクセルシェーダー: 画素1つにつき1回呼ばれ、「何色に塗るか」を決める
float4 PSMain(PSInput input) : SV_Target
{
    // 補間で長さが変わるので、法線を長さ 1 にそろえ直す
    float3 normal = normalize(input.worldNormal);

    // 陰影(Lambert): 面が光のほうを向くほど明るい。反対向き(内積がマイナス)は 0 = 光が当たらない
    float diffuse = saturate(dot(normal, toLight.xyz));

    // 明るさ = 環境光 + 光の色 × 当たり具合
    float3 lighting = ambientColor.rgb + lightColor.rgb * diffuse;

    // 画像から、この画素の uv の位置の色を取る(画像は sRGB 形式なので、GPU が、読むときに、光の計算用の値に直す)
    float4 albedo = albedoTexture.Sample(linearSampler, input.uv);

    // 頂点の色 × 画像の色 × 光
    float3 litColor = input.color.rgb * albedo.rgb * lighting;

    // フォグ: 遠い物ほど、霧の色に近づく。見える割合 = exp(-濃さ × 距離)(1 = くっきり、0 = 完全に霧の色)
    float distanceToCamera = length(input.worldPosition - cameraPosition.xyz);
    float visibility = exp(-fogParams.x * distanceToCamera);
    float3 finalColor = lerp(fogColor.rgb, litColor, visibility); // visibility = 1 なら litColor、0 なら fogColor

    return float4(finalColor, input.color.a * albedo.a);
}
