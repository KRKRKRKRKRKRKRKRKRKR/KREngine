---
title: Texture(画像)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Texture

## 概要
形に貼る**画像**(テクスチャ)。PNG などの画像ファイルを読んで、GPU に置く。ゲームは、**番号札**(`TextureHandle`)だけを持つ。形のどこに画像のどの部分を貼るかは、頂点の `uv`(`Vertex::uv`)で決まる。OBJ ファイルの `.mtl` に画像が書いてあれば、`MeshData::diffuseTexturePath` に、場所が入る。

画像を貼らない物は、白の 1 × 1 の画像が使われる(頂点の色のまま描かれる)。

ヘッダー:`KR/Renderer/Texture.h`(読み込み・捨てる関数は、`Renderer` にある)

## サンプルコード
OBJ モデルに、画像を貼って描く。

```cpp
void OnInit() override
{
    KR::MeshData data = KR::LoadObj("Test/Test.obj");              // .mtl に画像があれば、diffuseTexturePath に入る
    m_Model = GetRenderer().CreateMesh(data);

    if (!data.diffuseTexturePath.empty())
    {
        m_Texture = GetRenderer().CreateTexture(data.diffuseTexturePath);   // 起動時に読む(走りながら作ると、止まる)
    }
}
void OnRender() override
{
    GetRenderer().DrawMesh(m_Model, DirectX::XMMatrixIdentity(), m_Texture);   // 画像を渡す。無効な札なら、画像なしで描く
}
void OnShutdown() override
{
    GetRenderer().DestroyTexture(m_Texture);
    GetRenderer().DestroyMesh(m_Model);
}
```

## 関数

### Renderer::CreateTexture()　画像を読む
```cpp
TextureHandle CreateTexture(const std::filesystem::path& file);
```
画像ファイル(PNG など)を読んで、GPU に置き、番号札を返す。失敗したら、無効な番号札(`IsValid()` が `false`)。コピーが終わるまで待つので、**起動時に読む**。

| 引数 | 説明 |
|---|---|
| `file` | 画像ファイルのパス |

### Renderer::DestroyTexture()　画像を捨てる
```cpp
void DestroyTexture(TextureHandle texture);
```
番号札をすぐに無効にして、GPU が使い終わってから、実際に捨てる。

### Renderer::DrawMesh()　画像を貼って描く
```cpp
void DrawMesh(MeshHandle mesh, const DirectX::XMMATRIX& world, TextureHandle texture = {}, bool overlay = false);
```
第 3 引数に、画像の番号札を渡すと、その画像を貼って描く。→ [Renderer](Renderer.md)

## 列挙型・構造体

### TextureHandle　画像の番号札
`MeshHandle` と同じ考え方。実体(GPU のメモリ)は、`Renderer` が預かっている。

| メンバー | 説明 |
|---|---|
| `id` | 番号。0 は無効(まだ `CreateTexture` していない) |
| `IsValid()` | 有効な番号札か |

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `TextureHandle::IsValid()` | 番号札が有効か |
