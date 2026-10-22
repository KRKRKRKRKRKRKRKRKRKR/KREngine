---
title: Material(物ごとの質感)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-22
replaces:
---

# Material

## 概要
物ごとの「質感」。**トゥーン**(アニメ調の帯)の設定もここ。**ハイライト**(光が当たったところの、つやのある点)の、強さ・鋭さ・色と、物の色に掛ける色(拡散の色)を決める。`DrawMesh`・`DrawSkinnedMesh`・`AnimatedModel::Draw` に、**省略できる引数**として渡す。**渡さなければ、既定の材質 = 今までと同じ見た目**(ハイライトなし)。

ヘッダー:`KR/Renderer/Material.h`(`KR/KR.h` に入っている)

## サンプルコード
つやのある、赤いボールと、つやのない、地面。

```cpp
void OnInit() override
{
    m_Ball  = GetRenderer().CreateMesh(KR::CreateSphereMeshData(16, 32, { 0.9f, 0.2f, 0.2f, 1.0f }));
    m_Floor = GetRenderer().CreateMesh(KR::CreateCheckerPlaneMeshData(20, 20, 1.0f, { -10, 0, -10 },
                                       { 0.7f, 0.7f, 0.7f, 1 }, { 0.5f, 0.5f, 0.5f, 1 }));
    m_Shiny.specularStrength = 1.0f;   // ハイライトを出す(0 = 出さない)
    m_Shiny.shininess = 64.0f;         // 大きいほど、小さく鋭い点
}

void OnRender() override
{
    GetRenderer().DrawMesh(m_Floor, XMMatrixIdentity());                      // 材質を渡さない = 今までと同じ
    GetRenderer().DrawMesh(m_Ball, XMMatrixTranslation(0, 0.5f, 0), m_Shiny); // 材質つき
}

KR::Material m_Shiny;
```
動くモデルも、同じ:`m_Model.Draw(GetRenderer(), world, m_Shiny);`

## 関数
`Material` は、**値を入れるだけの構造体**で、関数は持たない。材質を渡す関数は、`Renderer` と `AnimatedModel` にある。

### Renderer::DrawMesh()(材質つき)
```cpp
void DrawMesh(MeshHandle mesh, const DirectX::XMMATRIX& world, const Material& material,
              TextureHandle texture = {}, bool overlay = false);
```
材質を渡して、形を描く。材質を渡さない形(`DrawMesh(mesh, world, texture, overlay)`)は、既定の材質で描く。→ [Renderer](Renderer.md)

### Renderer::DrawSkinnedMesh()(材質つき)
```cpp
void DrawSkinnedMesh(MeshHandle mesh, const DirectX::XMMATRIX& world,
                     std::span<const DirectX::XMFLOAT4X4> palette, const Material& material, TextureHandle texture = {});
```
普通は `AnimatedModel::Draw` が呼ぶ。

### AnimatedModel::Draw()(材質つき)
```cpp
void Draw(Renderer& renderer, const DirectX::XMMATRIX& world, const Material& material, TextureHandle texture = {}) const;
```
→ [AnimatedModel](AnimatedModel.md)

## 構造体

### Material
| メンバー | 既定値 | 説明 |
|---|---|---|
| `diffuseColor` | (1, 1, 1) | 物の色(頂点の色・画像)に掛ける色。(1, 1, 1) = そのまま。赤くしたい物は (1, 0.3, 0.3) など |
| `specularColor` | (1, 1, 1) | ハイライトの色。(1, 1, 1) = 光の色そのまま。金属のように、物の色を付けたいときは、その色にする |
| `shininess` | 32 | ハイライトの鋭さ。大きいほど、小さく鋭い点(プラスチック 32・金属 100 くらい。1〜256) |
| `specularStrength` | 0 | ハイライトの強さ。**0 = ハイライトなし**(既定)。1 で、はっきり出る |
| `toonSteps` | 0 | トゥーン(アニメ調)の帯の数。**0 = なめらか**(既定)・2〜16(1 は 0 と同じ)。光の当たり具合を、この数の帯に切る |
| `unlit` | false | **true で、光・影・フォグの影響を受けない**(物の色 × 画像 × `diffuseColor` が、そのまま画面の色になる)。画面に固定して出す UI の物(ゲージ・文字)用。夜の暗い光でも、決めた色のまま出る。ハイライト・トゥーンも効かない(0.18.0 から) |
| `toonSoftness` | 0.05 | 帯の境目のなめらかさ(0〜0.5)。0 = くっきり・大きいほど、ぼやける。トゥーンのときだけ効く |

### トゥーン(アニメ調)の使い方
```cpp
KR::Material toon;
toon.toonSteps = 3;               // 3 段階の帯(明・中・暗)
toon.toonSoftness = 0.03f;        // 境目は、ほぼくっきり
toon.specularStrength = 1.0f;     // ハイライトも、はっきりした点になる
GetRenderer().DrawMesh(m_Ball, world, toon);
```
帯にするのは**光の当たり具合**で、物の色(頂点の色・画像)・環境光は、そのまま。太陽・点光源・スポットライトの、**光ごと**に帯に切る。輪郭線(縁取り)と影は、まだ無い。

ハイライトは、**太陽・点光源・スポットライトの、全部の光**に対して出る(Blinn-Phong)。光が当たらない面には、出ない。反射(まわりの景色が映る)は、今は無い。

## アクセサー一覧
なし(構造体のメンバーを、直接書き換える)。