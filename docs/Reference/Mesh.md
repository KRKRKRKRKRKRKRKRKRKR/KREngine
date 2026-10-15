---
title: Mesh(形のデータ)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Mesh

## 概要
3D の**形のデータ**。頂点(位置・法線・色・画像の位置)と、番号(3 つで三角形 1 つ)でできている。`MeshData`(CPU 側のデータ)を作って、`Renderer::CreateMesh()` に渡すと、**番号札**(`MeshHandle`)が返る。エンジンが、キューブ・板・OBJ ファイルから、`MeshData` を作る関数を用意している。

ヘッダー:`KR/Renderer/Mesh.h`・`KR/Renderer/ModelLoader.h`

## サンプルコード
用意された形を使う例と、自分で三角形を 1 つ作る例。

```cpp
// ① 用意された形
KR::MeshHandle cube  = GetRenderer().CreateMesh(KR::CreateCubeMeshData());                    // 面ごとに色が違うキューブ
KR::MeshHandle red   = GetRenderer().CreateMesh(KR::CreateCubeMeshData({ 1, 0, 0, 1 }));      // 全部の面が赤
KR::MeshHandle floor = GetRenderer().CreateMesh(KR::CreateCheckerPlaneMeshData(
    8, 8, 1.0f, { -4.0f, 0.0f, -4.0f }, { 0.9f, 0.9f, 0.9f, 1.0f }, { 0.4f, 0.4f, 0.4f, 1.0f }));

// ② OBJ ファイル(Assets/Models/ からの相対パス)
KR::MeshHandle model = GetRenderer().CreateMesh(KR::LoadObj("Test/Test.obj"));
if (!model.IsValid()) { KR_LOG_ERROR("モデルを作れなかった"); }

// ③ 自分で作る:三角形 1 つ(外側から見て時計回り)
KR::MeshData tri;
tri.vertices = {
    { { 0, 0, 0 }, { 0, 0, -1 }, { 1, 0, 0, 1 } },   // 位置・法線・色
    { { 0, 1, 0 }, { 0, 0, -1 }, { 0, 1, 0, 1 } },
    { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 0, 1, 1 } },
};
tri.indices = { 0, 1, 2 };
KR::MeshHandle triangle = GetRenderer().CreateMesh(tri);
```

## 関数

### KR::CreateCubeMeshData()　キューブの形のデータ
```cpp
MeshData CreateCubeMeshData();
MeshData CreateCubeMeshData(const DirectX::XMFLOAT4& color);
```
1 辺 1 メートルで、中心が原点のキューブ。引数なしは、面ごとに色が違う(前:赤、後:水色、右:緑、左:紫、上:青、下:黄)。色を渡すと、全部の面が同じ色。

| 引数 | 説明 |
|---|---|
| `color` | 全部の面の色(r, g, b, a。0〜1) |

### KR::CreateCheckerPlaneMeshData()　市松模様の板
```cpp
MeshData CreateCheckerPlaneMeshData(uint32_t tilesX, uint32_t tilesZ, float tileSize,
                                    const DirectX::XMFLOAT3& origin,
                                    const DirectX::XMFLOAT4& colorA, const DirectX::XMFLOAT4& colorB);
```
平らな板(上向き)。`tileSize` メートル四方のタイルを、X 方向に `tilesX` 枚、Z 方向に `tilesZ` 枚、並べて、2 色を交互に塗る。

| 引数 | 説明 |
|---|---|
| `tilesX`, `tilesZ` | タイルの数(X 方向・Z 方向) |
| `tileSize` | タイル 1 枚の 1 辺(メートル) |
| `origin` | 板の「X・Z が最小の角」の位置(Y が板の高さ) |
| `colorA`, `colorB` | 交互に塗る 2 色 |

### KR::LoadObj()　OBJ ファイルを読む
```cpp
MeshData LoadObj(const std::filesystem::path& relativePath);
```
OBJ ファイルを読んで、形のデータにする。失敗したら、ログにエラーを出して、**空の MeshData** を返す(`CreateMesh` が、無効な番号札を返す)。`.mtl` に画像(`map_Kd`)があれば、`diffuseTexturePath` に入る。

| 引数 | 説明 |
|---|---|
| `relativePath` | `Assets/Models/` からの相対パス(例 `"Cube/Cube.obj"`) |

動く(骨つきの)モデルは、`LoadObj` ではなく、`AnimatedModel` を使う。→ [AnimatedModel](AnimatedModel.md)

## 列挙型・構造体

### Vertex　頂点 1 つ
| メンバー | 型 | 説明 |
|---|---|---|
| `position` | `XMFLOAT3` | 位置(メートル) |
| `normal` | `XMFLOAT3` | 法線(面が向いている方向。長さ 1。陰影の計算に使う) |
| `color` | `XMFLOAT4` | 色(r, g, b, a) |
| `uv` | `XMFLOAT2` | 画像のどこの色を使うか(u = 横、v = 縦。0〜1。左上が (0, 0))。画像を使わない形は (0, 0) |

### SkinVertex　骨に引っぱられる設定(頂点 1 つ分)
1 頂点に、最大 4 本の骨。普通は、モデルの読み込みが作る。

| メンバー | 説明 |
|---|---|
| `joints[4]` | 骨の番号(`Skeleton::joints` の何番目か)。使わない枠は、重み 0 |
| `weights[4]` | 重み(どれだけ引っぱられるか。4 つの合計が 1) |

### MeshData　形のデータ(CPU 側)
`CreateMesh` に渡したら、ゲーム側では捨ててよい。

| メンバー | 説明 |
|---|---|
| `vertices` | 頂点のリスト |
| `indices` | 番号のリスト(3 つで三角形 1 つ。**外側から見て時計回り**) |
| `skin` | 骨の設定。空なら骨なし。空でなければ、`vertices` と同じ数・同じ並び。骨つきの形は `DrawSkinnedMesh` で描く |
| `diffuseTexturePath` | 貼る画像のファイル(`.mtl` の `map_Kd`)。無ければ空。`LoadObj` が入れる |

### MeshHandle　形の番号札
実体(GPU のバッファ)は、`Renderer` が預かっている。

| メンバー | 説明 |
|---|---|
| `id` | 番号。0 は無効(まだ `CreateMesh` していない) |
| `IsValid()` | 有効な番号札か |

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `MeshHandle::IsValid()` | 番号札が有効か |
