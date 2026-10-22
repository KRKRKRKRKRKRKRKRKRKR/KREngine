---
title: Renderer(描画)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-22
replaces:
---

# Renderer

## 概要
形・画像を描く窓口。中身は DirectX 12 だが、ゲームは DirectX を意識しなくてよい。ゲームは、**番号札**(`MeshHandle`・`TextureHandle`)だけを持つ。

**描くのは、積むだけ**:`DrawMesh` などは、その場では描かない。`OnRender` の中で「この形を、この行列で描いて」という依頼を積み、フレームの最後に、エンジンが、まとめて描く。

座標系は**左手系**(+X が右・+Y が上・+Z が奥)。長さの単位は**メートル**。行列は DirectXMath の `XMMATRIX` で、`拡大 × 回転 × 移動` の順に掛ける。

`Renderer` は、`Application::GetRenderer()` で取る。自分で作らない。

ヘッダー:`KR/Renderer/Renderer.h`

## サンプルコード
形を作って、画像を貼って、線で当たり判定の箱を見せる。

```cpp
#include <KR/KR.h>
using namespace DirectX;

class MyGame : public KR::Application
{
protected:
    void OnInit() override
    {
        GetRenderer().SetClearColor(0.1f, 0.12f, 0.16f);                       // 背景色
        m_Cube  = GetRenderer().CreateMesh(KR::CreateCubeMeshData({ 0.9f, 0.4f, 0.2f, 1.0f }));
        m_Floor = GetRenderer().CreateMesh(KR::CreateCheckerPlaneMeshData(
            10, 10, 1.0f, { -5.0f, 0.0f, -5.0f }, { 0.8f, 0.8f, 0.8f, 1.0f }, { 0.3f, 0.3f, 0.3f, 1.0f }));
    }
    void OnUpdate(float dt) override
    {
        m_Angle += dt;

        KR::Camera camera;                                                      // カメラ・光は、毎フレーム渡す
        camera.SetPosition({ 0.0f, 3.0f, -6.0f });
        camera.LookAt({ 0.0f, 0.5f, 0.0f });
        GetRenderer().SetCamera(camera);
        GetRenderer().SetLighting(KR::Lighting{});
    }
    void OnRender() override
    {
        GetRenderer().DrawMesh(m_Floor, XMMatrixIdentity());
        const XMMATRIX world = XMMatrixRotationY(m_Angle) * XMMatrixTranslation(0.0f, 0.5f, 0.0f);
        GetRenderer().DrawMesh(m_Cube, world);

        // 当たり判定の箱を、線で見る
        KR::OBB box = KR::MakeOBB({ 0.0f, 0.5f, 0.0f }, { 0.5f, 0.5f, 0.5f }, XMMatrixRotationY(m_Angle));
        GetRenderer().DrawWireBox(box, { 0.0f, 1.0f, 0.0f, 1.0f });
    }
    void OnShutdown() override
    {
        GetRenderer().DestroyMesh(m_Cube);
        GetRenderer().DestroyMesh(m_Floor);
    }
private:
    KR::MeshHandle m_Cube, m_Floor;
    float m_Angle = 0.0f;
};
```

## 関数

### Renderer::SetClearColor()　背景色を決める
```cpp
void SetClearColor(float r, float g, float b);
```
画面を塗りつぶす色(0〜1)。空(`SetSky`)を使うと、空が先に描かれる。

### Renderer::SetVSync()　垂直同期を切り替える
```cpp
void SetVSync(bool enabled);
```
`true`(既定)で、モニターの更新に合わせる。`false` で、できるだけ速く描く。

### Renderer::SetCamera()　カメラを渡す
```cpp
void SetCamera(const Camera& camera);
```
このフレームで使うカメラ(コピーして保持する)。呼ばなければ、`Camera` の初期値で描く。**毎フレーム**、`OnUpdate` で渡す。→ [Camera](Camera.md)

### Renderer::SetLighting()　光を渡す
```cpp
void SetLighting(const Lighting& lighting);
```
このフレームで使う光(太陽の向き・色・環境光)。呼ばなければ、初期値。→ [Lighting](Lighting.md)

### Renderer::SetSky()　空を渡す
```cpp
void SetSky(const Sky& sky);
```
このフレームで使う空。画面全体に、地平線から天頂へのグラデーションを、最初に描く。

### Renderer::SetFog()　霧を渡す
```cpp
void SetFog(const Fog& fog);
```
このフレームで使う霧。霧の色は、空の地平線の色に、自動で合わせる。

### Renderer::CreateMesh()　形を GPU に置く
```cpp
MeshHandle CreateMesh(const MeshData& data);
```
形のデータを GPU のバッファに入れて、番号札を返す。失敗したら、無効な番号札(`IsValid()` が `false`)。渡した `MeshData` は、ゲーム側では捨ててよい。

| 引数 | 説明 |
|---|---|
| `data` | 形のデータ。`CreateCubeMeshData()`・`LoadObj()` などで作る → [Mesh](Mesh.md) |

### Renderer::DestroyMesh()　形を捨てる
```cpp
void DestroyMesh(MeshHandle mesh);
```
番号札を、すぐに無効にして、GPU が使い終わってから、実際に捨てる。`OnShutdown` で呼ぶ。

### Renderer::CreateTexture()　画像を読む
```cpp
TextureHandle CreateTexture(const std::filesystem::path& file);
```
画像ファイル(PNG など)を読んで、GPU に置き、番号札を返す。失敗したら、無効な番号札。コピーが終わるまで待つので、**起動時(`OnInit`)に読む**(走りながら作ると、止まる)。→ [Texture](Texture.md)

### Renderer::DestroyTexture()　画像を捨てる
```cpp
void DestroyTexture(TextureHandle texture);
```
`DestroyMesh` と同じ。番号札を無効にして、遅れて実際に捨てる。

### Renderer::DrawMesh()　形を描く
```cpp
void DrawMesh(MeshHandle mesh, const DirectX::XMMATRIX& world,
              TextureHandle texture = {}, bool overlay = false);
```
形を描く依頼を、描画リストに積む(`OnRender` で呼ぶ)。

| 引数 | 説明 |
|---|---|
| `mesh` | 描く形の番号札 |
| `world` | 置き方の行列(拡大・回転・移動) |
| `texture` | 貼る画像。省略すると、白(頂点の色のまま) |
| `overlay` | `true` で、深度テストなしで、ほかの物の**手前**に描く(物の中にある骨を見せるときなど) |

**材質つき**(0.16.0 から):
```cpp
void DrawMesh(MeshHandle mesh, const DirectX::XMMATRIX& world, const Material& material,
              TextureHandle texture = {}, bool overlay = false);
```
`material` で、ハイライトの強さ・鋭さ・色と、物の色への掛け色を決める。材質を渡さない形は、既定の材質(今までと同じ見た目)。→ [Material](Material.md)

### Renderer::DrawSkinnedMesh()　骨つきの形を描く
```cpp
void DrawSkinnedMesh(MeshHandle mesh, const DirectX::XMMATRIX& world,
                     std::span<const DirectX::XMFLOAT4X4> palette, TextureHandle texture = {});
```
骨つきの形(`MeshData::skin` を持つ形)を、骨の行列の表(パレット)で動かして描く。普通は、`AnimatedModel::Draw()` が呼ぶので、**ゲームは、直接呼ばない**。材質を渡す形(`palette` の次に `const Material& material`)も、ある。骨は 512 本まで・1 フレームに 32 個まで。`overlay` は使えない。→ [AnimatedModel](AnimatedModel.md)

### Renderer::DrawLine()　線を引く
```cpp
void DrawLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
              const DirectX::XMFLOAT4& color, bool overlay = false);
```
デバッグ用の線を 1 本、積む。座標は世界の座標。光・霧はかからず、色のまま描く。1 フレームに、約 3 万本まで。

### Renderer::DrawWireBox()　箱を線で描く
```cpp
void DrawWireBox(const OBB& box, const DirectX::XMFLOAT4& color);
```
`OBB` の 12 本の辺を、線で積む。当たり判定の箱の確認用。

### Renderer::GetViewExtent()　カメラの前の面の大きさ
```cpp
ViewExtent GetViewExtent(float distance) const; // struct ViewExtent { float halfWidth; float halfHeight; }
```
カメラの前 `distance` メートルの面が、画面いっぱいに見える大きさ(半分の幅・半分の高さ。メートル)。今のカメラの視野・ウィンドウの縦横比から求める。**画面に貼り付ける物**(`Material::cameraSpace`)の位置を決めるのに使う。右端 = `(halfWidth, 0, distance)`・上端 = `(0, halfHeight, distance)`。→ [Material](Material.md)

```cpp
KR::Material material;
material.cameraSpace = true;                        // 渡す行列は、カメラの座標
const auto extent = GetRenderer().GetViewExtent(10.0f);
GetRenderer().DrawMesh(mesh, XMMatrixTranslation(extent.halfWidth * 0.9f, 0.0f, 10.0f), material, {}, true); // 画面の右端(の少し内側)・カメラの前 10 m
```

### Renderer::DrawLightGizmo() / DrawLightGizmos()　光の範囲を線で描く
光の位置と届く範囲を、光の色の線で描く(Debug だけ)。→ [Lighting](Lighting.md)

### Renderer::DrawWireSphere()　球を線で描く
```cpp
void DrawWireSphere(const Sphere& sphere, const DirectX::XMFLOAT4& color);
```
球を、3 つの円(XY・YZ・XZ の面)の線で積む(72 本)。当たり判定の球の確認用。

## 列挙型・構造体

### Renderer::MaxSkinJoints　骨の本数の上限
```cpp
static constexpr uint32_t MaxSkinJoints = 512;
```
骨つきの形が持てる骨の数の上限。

### Renderer::MaxSkinnedDrawsPerFrame　骨つきの形の数の上限
```cpp
static constexpr uint32_t MaxSkinnedDrawsPerFrame = 32;
```
1 フレームに描ける、骨つきの物の数。超えた分は、警告を出して、描かない。

## アクセサー一覧
`Renderer` の、ゲームが使う設定の関数は、すべて上の「関数」の中にある。

| 関数 | 説明 |
|---|---|
| `SetClearColor()` | 背景色 |
| `SetVSync()` | 垂直同期 |
| `SetCamera()` | カメラ |
| `SetLighting()` | 光 |
| `SetSky()` | 空 |
| `SetFog()` | 霧 |
