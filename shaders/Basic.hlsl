// b0 の箱から受け取るデータ(C++ 側は SetGraphicsRoot32BitConstants で入れる)
cbuffer ObjectConstants : register(b0)
{
    // row_major: DirectXMath と同じく、行列を「横に読む」
    row_major float4x4 world; // ワールド行列(物体の拡大・回転・移動)
};

// b1 の別紙(フレームごとのデータ。カメラ・光・フォグ)の宣言は、Sky.hlsl と共有するので、共通のファイルにある
#include "Common.hlsli"

// b3 の箱から受け取る、物ごとの材質(C++ 側は Renderer.cpp の MaterialConstants。ピクセルシェーダーだけが読む)
cbuffer MaterialConstants : register(b3)
{
    float4 materialDiffuse;  // rgb = 物の色に掛ける色
    float4 materialSpecular; // rgb = ハイライトの色 / w = ハイライトの強さ
    float4 materialParams;   // x = ハイライトの鋭さ / y = トゥーンの段階の数(0 = なめらか) / z = 帯の境目のなめらかさ / w = 旗(1 = 光を受けない(unlit)・2 = カメラ固定(cameraSpace)。足し算)
};

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

// 骨の行列の表(パレット)。スキニングの頂点シェーダー(VSSkinned)だけが読む(C++ 側は Root Signature の b2 の口)
// 各行列 = 逆バインド行列 × 骨の今の世界の行列。MaxJoints は、C++ 側の MaxSkinJoints(Renderer.cpp)と同じにする
static const uint MaxJoints = 512;
cbuffer SkinPalette : register(b2)
{
    row_major float4x4 bones[MaxJoints];
};

// ローカルの位置・法線を、ワールド → 画面に変換する(骨なしの VSMain も、骨つきの VSSkinned も、最後にこれを通る)
PSInput TransformVertex(float3 localPosition, float3 localNormal, float4 color, float2 uv)
{
    PSInput output;
    // w = 1(位置)にしてから行列を掛ける → 拡大・回転・移動がすべて効く
    float4 worldPosition = mul(float4(localPosition, 1.0), world); // ① ローカル → ワールド
    // カメラ固定の材質(cameraSpace。旗の 2):渡された行列は、ワールドではなく、カメラの座標。ビューを掛けずに、プロジェクションだけを掛ける
    //   (ワールドを経由しない = 世界の座標が大きくても、ぶれない。光・フォグは、受けない材質なので、worldPosition の値は、使われない)
    const bool cameraSpace = ((uint)(materialParams.w + 0.5)) & 2;
    output.position = cameraSpace ? mul(worldPosition, projection)    // ② カメラの座標 → 画面
                                  : mul(worldPosition, viewProjection); // ② ワールド → カメラ → 画面
    // この後 GPU が自動で x, y を w(奥行き)で割る → 遠い物ほど小さく映る
    output.worldPosition = worldPosition.xyz;

    // 法線は「向き」なので、w = 0 にして行列を掛ける → 回転と拡大だけが効き、移動は効かない
    // (長さは変わることがあるので、ピクセルシェーダーで 1 にそろえ直す)
    output.worldNormal = mul(float4(localNormal, 0.0), world).xyz;

    output.color = color;
    output.uv = uv;
    return output;
}

// 頂点シェーダー: 頂点1つにつき1回呼ばれ、「画面のどこに置くか」を決める
PSInput VSMain(VSInput input)
{
    return TransformVertex(input.position, input.normal, input.color, input.uv);
}

// 骨つきの頂点シェーダーへの入力。VSInput に、骨の番号(4 つ)と重み(4 つ)を足した形
struct VSSkinnedInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD;
    uint4  joints   : BLENDINDICES; // 引っぱる骨の番号(パレットの何番目か)
    float4 weights  : BLENDWEIGHT;  // それぞれの重み(合計 1)
};

// 骨つきの頂点シェーダー: 4 本の骨の行列を、重みで混ぜて、頂点を動かす → そのあとは、骨なしと同じ
PSInput VSSkinned(VSSkinnedInput input)
{
    float4x4 skin = bones[input.joints.x] * input.weights.x
                  + bones[input.joints.y] * input.weights.y
                  + bones[input.joints.z] * input.weights.z
                  + bones[input.joints.w] * input.weights.w;

    // 位置(w = 1)も、法線(w = 0)も、混ぜた行列で、骨の動きの分だけ動かす
    float3 skinnedPosition = mul(float4(input.position, 1.0), skin).xyz;
    float3 skinnedNormal = mul(float4(input.normal, 0.0), skin).xyz;
    return TransformVertex(skinnedPosition, skinnedNormal, input.color, input.uv);
}

// ピクセルシェーダー: 画素1つにつき1回呼ばれ、「何色に塗るか」を決める
float4 PSMain(PSInput input) : SV_Target
{
    // 画像から、この画素の uv の位置の色を取る(画像は sRGB 形式なので、GPU が、読むときに、光の計算用の値に直す)
    float4 albedo = albedoTexture.Sample(linearSampler, input.uv);

    // 光を受けない材質(unlit。画面に固定して出す UI の物): 光・ハイライト・フォグを飛ばして、色をそのまま出す
    if (((uint)(materialParams.w + 0.5)) & 1) // 旗の 1 = 光を受けない
    {
        return float4(input.color.rgb * albedo.rgb * materialDiffuse.rgb, input.color.a * albedo.a);
    }
    // 補間で長さが変わるので、法線を長さ 1 にそろえ直す
    float3 normal = normalize(input.worldNormal);

    // 陰影(Lambert): 面が光のほうを向くほど明るい。反対向き(内積がマイナス)は 0 = 光が当たらない
    // トゥーン(材質の段階の数が 2 以上のとき): 当たり具合を、段階の帯に切る。0 のときは、Toon は、そのまま返す
    float toonSteps = materialParams.y;
    float toonSoftness = materialParams.z;
    float diffuse = Toon(saturate(dot(normal, toLight.xyz)), toonSteps, toonSoftness);

    // 明るさ = 環境光 + 光の色 × 当たり具合
    float3 lighting = ambientColor.rgb + lightColor.rgb * diffuse;

    // ハイライトに使う光の合計(太陽 + 点光源 + スポットライト。強さ・色は、あとで、材質を掛ける)
    float3 viewDirection = normalize(cameraPosition.xyz - input.worldPosition);
    float shininess = materialParams.x;
    float3 specularLight = lightColor.rgb * ToonSpecular(BlinnPhong(normal, toLight.xyz, viewDirection, shininess), toonSteps, toonSoftness);

    // 点光源: 光ごとに、「距離による弱まり × 当たり具合」を足す
    for (uint i = 0; i < (uint)lightCounts.x; ++i)
    {
        float3 toPoint = pointPositionRange[i].xyz - input.worldPosition;
        float distanceToLight = length(toPoint);
        float3 direction = toPoint / max(distanceToLight, 0.0001);
        float3 lightRgb = pointColor[i].rgb * Attenuation(distanceToLight, pointPositionRange[i].w);
        lighting += lightRgb * Toon(saturate(dot(normal, direction)), toonSteps, toonSoftness);
        specularLight += lightRgb * ToonSpecular(BlinnPhong(normal, direction, viewDirection, shininess), toonSteps, toonSoftness);
    }

    // スポットライト: 点光源と同じ + 円錐の中での強さ
    for (uint j = 0; j < (uint)lightCounts.y; ++j)
    {
        float3 toSpot = spotPositionRange[j].xyz - input.worldPosition;
        float distanceToLight = length(toSpot);
        float3 direction = toSpot / max(distanceToLight, 0.0001);
        float cone = SpotFactor(direction, spotDirectionOuter[j].xyz, spotDirectionOuter[j].w, spotColorInner[j].w);
        float3 lightRgb = spotColorInner[j].rgb * (Attenuation(distanceToLight, spotPositionRange[j].w) * cone);
        lighting += lightRgb * Toon(saturate(dot(normal, direction)), toonSteps, toonSoftness);
        specularLight += lightRgb * ToonSpecular(BlinnPhong(normal, direction, viewDirection, shininess), toonSteps, toonSoftness);
    }


    // 頂点の色 × 画像の色 × 光
    float3 litColor = input.color.rgb * albedo.rgb * materialDiffuse.rgb * lighting;

    // ハイライト(光の反射): 物の色には掛けず、光の合計 × 鏡面の色 を、足す(強さ 0 なら、何も足さない = 今までの見た目)
    litColor += specularLight * materialSpecular.rgb * materialSpecular.w;

    // フォグ: 遠い物ほど、霧の色に近づく。見える割合 = exp(-濃さ × 距離)(1 = くっきり、0 = 完全に霧の色)
    float distanceToCamera = length(input.worldPosition - cameraPosition.xyz);
    float visibility = exp(-fogParams.x * distanceToCamera);
    float3 finalColor = lerp(fogColor.rgb, litColor, visibility); // visibility = 1 なら litColor、0 なら fogColor

    return float4(finalColor, input.color.a * albedo.a);
}
