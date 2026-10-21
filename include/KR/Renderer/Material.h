#pragma once

#include <DirectXMath.h>

namespace KR
{
    // 物ごとの「質感」。DrawMesh / DrawSkinnedMesh / AnimatedModel::Draw に渡す(渡さなければ、既定の材質 = 今までの見た目)
    // 光の当たり方(ハイライトの強さ・鋭さ・色)を物ごとに変える。形の色(頂点の色・画像)は、これとは別
    struct Material
    {
        // 物の色に掛ける色(拡散の色)。(1, 1, 1) = そのまま。赤くしたい物は (1, 0.3, 0.3) など
        DirectX::XMFLOAT3 diffuseColor = { 1.0f, 1.0f, 1.0f };

        // ハイライト(光の反射のつや)の色。(1, 1, 1) = 光の色そのまま。金属のように、物の色を付けたいときは、その色にする
        DirectX::XMFLOAT3 specularColor = { 1.0f, 1.0f, 1.0f };

        // ハイライトの鋭さ。大きいほど、小さく鋭い点になる(プラスチック 32・金属 100 くらい。1 〜 256)
        float shininess = 32.0f;

        // ハイライトの強さ。0 = ハイライトなし(既定。今までと同じ見た目)。1 で、はっきり出る
        float specularStrength = 0.0f;
    };
}
