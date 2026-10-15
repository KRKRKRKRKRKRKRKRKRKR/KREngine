---
title: ModelLoader(モデルの読み込み)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# ModelLoader

## 概要
モデルのファイル(glTF・FBX)を読んで、形・骨・動きの**データ**(`ModelData`)にする部品。**普通は、`AnimatedModel::Load()` が使うので、ゲームは、直接呼ばない**。モデルの読み込みを、自分で細かく扱いたいとき(骨や動きのデータを、直接見たいときなど)に使う。

- `LoadModel()` は、拡張子で、`.glb` / `.gltf` → `LoadGltf()`、`.fbx` → `LoadFbx()` を選ぶ。拡張子の大文字・小文字は、区別しない。
- どちらも、**このエンジンの座標系**(左手系・Y が上・メートル)に直して返す。
- 失敗したら、ログにエラーを出して、**空の ModelData** を返す(落ちない)。`IsEmpty()` で調べる。
- 読むライブラリ(cgltf・ufbx)の型は、公開ヘッダーに出さない。

ヘッダー:`KR/Animation/LoadModel.h`・`GltfLoader.h`・`FbxLoader.h`・`Skeleton.h`・`AnimationClip.h`

## サンプルコード
モデルを読んで、骨の数と動きの名前を、ログに出す。

```cpp
KR::ModelData model = KR::LoadModel("Boss/Boss.glb");          // .fbx でも、同じ呼び方
if (model.IsEmpty())
{
    KR_LOG_ERROR("モデルを読めなかった");
    return;
}
KR_LOG_INFO("頂点 {}・骨 {}・動き {}", model.mesh.vertices.size(), model.skeleton.joints.size(), model.clips.size());
for (const KR::AnimationClip& clip : model.clips)
{
    KR_LOG_INFO("動き: {}({} 秒)", clip.name, clip.duration);
}
KR::MeshHandle mesh = GetRenderer().CreateMesh(model.mesh);    // 形だけ、使うこともできる
```

## 関数

### KR::LoadModel()　拡張子で振り分けて読む
```cpp
ModelData LoadModel(const std::filesystem::path& relativePath);
```
モデルのファイルを、拡張子で振り分けて読む。知らない拡張子・読めないファイルは、ログにエラーを出して、空の `ModelData` を返す。

| 引数 | 説明 |
|---|---|
| `relativePath` | `Assets/Models/` からの相対パス |

### KR::LoadGltf()　glTF を読む
```cpp
ModelData LoadGltf(const std::filesystem::path& relativePath);
```
glTF(`.glb` / `.gltf`)を読む。骨の番号と重み(1 組目)は、`mesh.skin` に入る(骨つきなら)。メッシュのノードの変換は、使わない。

### KR::LoadFbx()　FBX を読む
```cpp
ModelData LoadFbx(const std::filesystem::path& relativePath);
```
FBX(`.fbx`)を読む。メッシュ(三角形・法線・UV)・骨(名前・親・基準姿勢・逆バインド)・重み(1 頂点 4 本まで)・動き(1 秒 30 回の直線補間に焼き付ける)を読む。**モデルの座標 = FBX の世界の座標**(Blender の置き場所を、そのまま持つ)。骨とメッシュが、大きく離れているファイルは、警告を出す(Blender で、原点にそろえて、書き出し直す)。

### KR::RunFbxSkinSelfTest()　骨の重みの処理の確認
```cpp
bool RunFbxSkinSelfTest();
```
確認用。5 本以上の骨が付いた頂点を、大きい順の 4 本に絞る処理などを、偽のデータで検査する。全部合っていれば `true`(結果は、ログに出る)。

## 列挙型・構造体

### ModelData　読んだモデル(CPU 側)
| メンバー | 説明 |
|---|---|
| `mesh` | 形のデータ(`MeshData`)。骨つきなら、`mesh.skin` に重みが入る → [Mesh](Mesh.md) |
| `skeleton` | 骨の階層(`Skeleton`)。骨が無いモデルは、空 |
| `clips` | 動きの一覧(`AnimationClip`) |
| `IsEmpty()` | 形が空か(読めなかったとき `true`) |

### Skeleton　骨の階層全体
| メンバー | 説明 |
|---|---|
| `joints` | 骨(`Joint`)の並び。頂点が持つ「骨の番号」は、この並びの番号 |
| `IsEmpty()` | 骨が無いか |

### Joint　骨 1 本
| メンバー | 説明 |
|---|---|
| `name` | 骨の名前 |
| `parent` | 親の骨の番号。−1 は根 |
| `parentWorld` | 根だけ:親のノード(Blender のアーマチュアなど)の世界の行列 |
| `translation`, `rotation`, `scale` | 基準姿勢(親の骨から見た、位置・向き・大きさ) |
| `inverseBind` | 逆バインド行列(頂点を、この骨の中の座標に戻す) |

### AnimationClip　1 つの動き
| メンバー | 説明 |
|---|---|
| `name` | 動きの名前(走る・羽ばたく、など) |
| `duration` | 長さ(秒) |
| `tracks` | 骨ごとの動き(`JointTrack`。位置・向き・大きさのキー列) |

### Interpolation　キーとキーの間の埋め方
| 値 | 説明 |
|---|---|
| `Linear` | 直線でつなぐ(向きは、球面で補間) |
| `Step` | 次のキーまで、前の値のまま(階段) |

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `ModelData::IsEmpty()` | 形が空か |
| `Skeleton::IsEmpty()` | 骨が無いか |
