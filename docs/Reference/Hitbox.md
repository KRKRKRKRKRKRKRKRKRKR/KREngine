---
title: Hitbox(骨にくっついた判定の形)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Hitbox

## 概要
動くモデルの、**骨にくっついた判定の形**(箱・球)。位置・向き・大きさは、**骨の中の座標**で持つ(骨が動いても、この数字は変わらない)。ゲームは、`AnimatedModel::GetWorldHitboxes()` で、今の姿勢での、世界の形を取って、`Intersects()` で調べる。この文書の関数は、その部品。**普通は、`AnimatedModel` だけ使えばよい**。

- 判定の形は、ツール `KRAnimationEditor` で調整して、`<モデル名>.hitboxes.json` に保存する。ゲームは、読むだけ。
- 骨は、**名前**で探す(無ければ番号)。骨の並びが変わっても、壊れない。
- 箱は `OBB`・球は `Sphere`(→ [Collision](Collision.md))。

ヘッダー:`KR/Animation/Hitbox.h`・`HitboxFile.h`・`HitboxMirror.h`

## サンプルコード
プレイヤーの攻撃(球)が、敵のどの部位に当たったかを調べる。

```cpp
const DirectX::XMMATRIX enemyWorld = DirectX::XMMatrixTranslation(m_EnemyPos.x, m_EnemyPos.y, m_EnemyPos.z);

KR::WorldHitbox attack;
attack.shape  = KR::HitShape::Sphere;
attack.sphere = KR::Sphere{ m_SwordTipPos, 0.3f };

const std::vector<KR::Hitbox>& defs  = m_Enemy.GetHitboxes();                  // 定義(名前・骨・形)
const std::vector<KR::WorldHitbox> now = m_Enemy.GetWorldHitboxes(enemyWorld); // 今の姿勢の、世界の形(defs と同じ並び)
for (size_t i = 0; i < now.size(); ++i)
{
    if (KR::Intersects(now[i], attack)) { KR_LOG_INFO("当たった部位: {}", defs[i].name); }
}
```

## 関数

### KR::EvaluateHitboxes()　判定の形を、世界の座標にする
```cpp
std::vector<WorldHitbox> EvaluateHitboxes(const std::vector<Hitbox>& hitboxes,
                                          const std::vector<DirectX::XMFLOAT4X4>& pose,
                                          const DirectX::XMMATRIX& modelWorld);
```
判定の形を、世界の座標にする。結果は、`hitboxes` と同じ数・同じ並び。世界の行列 = (骨の中の向き・位置) × 骨の今の行列 × モデル全体の行列。拡大・縮小がかかっても、箱の軸は長さ 1 のまま、大きさに掛かる。球の半径には、いちばん大きい拡大が掛かる(**一様な拡大だけ、正しい**)。`jointIndex` が範囲の外なら、骨の動きなし。

| 引数 | 説明 |
|---|---|
| `hitboxes` | 判定の形の定義 |
| `pose` | 骨の今の世界の行列(`AnimatedModel::GetPose()`) |
| `modelWorld` | モデル全体の置き方(`Draw` に渡す行列と同じ) |

### KR::Intersects()　2 つの判定の形が重なっているか
```cpp
bool Intersects(const WorldHitbox& a, const WorldHitbox& b);
```
形の種類(箱・球)の、4 つの組み合わせを、振り分けて調べる。

### KR::GenerateDefaultHitboxes()　既定の形を作る
```cpp
std::vector<Hitbox> GenerateDefaultHitboxes(const Skeleton& skeleton, float thickness);
```
とりあえずの判定の形を、骨ごとに作る(あとで、調整する前提)。子の骨がある骨は、子の骨までを覆う箱。子の骨が無い骨(先端)は、半径 `thickness` の球。

### KR::GetHitboxFilePath()　判定のファイルの場所
```cpp
std::filesystem::path GetHitboxFilePath(const std::filesystem::path& modelRelativePath);
```
モデルのファイル(例 `"Boss/Boss.glb"`)に対応する、判定の形のファイルの場所(例 `"Boss/Boss.hitboxes.json"`)。`Assets/Models/` からの相対パス。

### KR::SaveHitboxes()　判定の形を保存する
```cpp
bool SaveHitboxes(const std::filesystem::path& relativePath, const std::vector<Hitbox>& hitboxes,
                  const Skeleton& skeleton);
```
判定の形を、JSON ファイルに保存する。骨は、名前と番号の両方を書く。Debug では、元の `Assets` フォルダーに書く。失敗したら、エラーを出して `false`。

### KR::LoadHitboxes()　判定の形を読む
```cpp
HitboxLoadResult LoadHitboxes(const std::filesystem::path& relativePath, const Skeleton& skeleton,
                              std::vector<Hitbox>& out);
```
JSON ファイルから、判定の形を読む。骨は、名前で探す(無ければ番号。それも範囲外なら、警告して、その形を飛ばす)。

| 戻り値 | 説明 |
|---|---|
| `Loaded` | 読めた(`out` に入った) |
| `NotFound` | ファイルが無い(エラーではない。呼ぶ側が、既定の形を使う) |
| `Invalid` | 壊れている・形式が違う(エラーを出した。`out` は空) |

### KR::MirrorHitbox()　左右反転して、対の骨に付け直す
```cpp
Hitbox MirrorHitbox(const Hitbox& source, const DirectX::XMFLOAT4X4& sourceBoneWorld,
                    int targetJoint, const DirectX::XMFLOAT4X4& targetBoneWorld);
```
判定の形を、モデルの座標の X = 0 で左右反転して、対になる骨に付け直す(調整ツールの「左右反転して貼り付け」で使う)。骨の行列は、どちらも、同じ姿勢(普通は、基準姿勢)の物を渡す。

### KR::MirrorBoneName()　対の骨の名前
```cpp
std::optional<std::string> MirrorBoneName(const std::string& name);
```
対になる骨の名前(左 ⇔ 右)。`.L / .R`・`_L / _R`・`Left / Right`・`左 / 右` の決まりに当てはまる名前。どれにも当てはまらなければ `std::nullopt`。

### KR::FindMirrorJoint()　対の骨の番号
```cpp
std::optional<int> FindMirrorJoint(const Skeleton& skeleton, int joint);
```
骨 `joint` の、対になる骨の番号。`Skeleton` の中に無ければ `std::nullopt`。

## 列挙型・構造体

### HitShape　形の種類
| 値 | 説明 |
|---|---|
| `Box` | 箱(`OBB`。骨の中で、向きを持てる) |
| `Sphere` | 球 |

### Hitbox　判定の形の定義
骨にくっつけた、判定の形 1 つ。

| メンバー | 既定値 | 説明 |
|---|---|---|
| `name` | | 名前(例 `"Head"`) |
| `jointIndex` | −1 | くっつける骨(`Skeleton::joints` の番号) |
| `shape` | `Box` | 形の種類 |
| `center` | (0, 0, 0) | 骨の中での、中心の位置(骨の根もとが原点) |
| `rotation` | (0, 0, 0, 1) | 骨の中での、箱の向き(クォータニオン)。球では使わない |
| `halfSize` | (0.5, 0.5, 0.5) | 箱の、半分の大きさ(骨の中の軸の向き。メートル) |
| `radius` | 0.5 | 球の半径(メートル) |

### WorldHitbox　世界の座標での形
ある時刻の、世界の座標での形(判定に使う)。

| メンバー | 説明 |
|---|---|
| `shape` | 形の種類 |
| `box` | `shape` が `Box` のときに有効な `OBB` |
| `sphere` | `shape` が `Sphere` のときに有効な `Sphere` |

### HitboxLoadResult　読み込みの結果
`Loaded`・`NotFound`・`Invalid`(上の `LoadHitboxes()` を参照)。

## アクセサー一覧
なし(構造体のメンバーを、直接読み書きする)。`AnimatedModel::GetHitboxes()` で、定義を取れる。
