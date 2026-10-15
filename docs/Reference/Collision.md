---
title: Collision(当たり判定)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Collision

## 概要
当たり判定。**箱**(`OBB`。向きを持てる箱)・**球**(`Sphere`)・**光線**(`Ray`)を使って、「重なっているか」「光線が当たるか」を調べる。回転しない箱も、軸を (1,0,0)(0,1,0)(0,0,1) にすれば、同じ形で持てる。単位はメートル、座標は世界の座標。

- 箱どうし・球どうし・球と箱は、`Intersects()` で調べる(`bool`)。
- 光線は、`Raycast()` で、球・箱に当てる。当たれば、始点からの距離 `t` が返る(`std::optional<float>`)。
- 動くモデルの判定は、骨にくっついた形(`Hitbox`)を使う。→ [Hitbox](Hitbox.md)
- エンジンは、**当たったあとの動き(押し戻し・ダメージ)を持たない**。調べるだけで、どうするかは、ゲームが決める。

ヘッダー:`KR/Physics/Collision.h`・`KR/Physics/Ray.h`

## サンプルコード
プレイヤーの箱と敵の球が重なったら、ダメージ。マウスの光線が、箱に当たったかも調べる。

```cpp
using namespace DirectX;

// 箱:中心・半分の大きさ・回転の行列(見た目の回転と、同じ物を渡すと、向きがそろう)
KR::OBB player = KR::MakeOBB(m_PlayerPos, { 0.4f, 0.9f, 0.4f }, XMMatrixRotationY(m_PlayerYaw));
// 球:中心と半径
KR::Sphere enemy{ m_EnemyPos, 0.6f };

if (KR::Intersects(player, enemy))                       // 球と箱(どちらの順でもよい)
{
    KR_LOG_INFO("当たった");
}

// 目で見る
GetRenderer().DrawWireBox(player, { 0, 1, 0, 1 });
GetRenderer().DrawWireSphere(enemy, { 1, 0, 0, 1 });

// マウスの光線 → 箱
const KR::MousePosition m = KR::Input::GetMousePosition();
const KR::Ray ray = camera.ScreenPointToRay(m.x, m.y, 1280.0f, 720.0f);
if (const std::optional<float> t = KR::Raycast(ray, player))
{
    KR_LOG_INFO("箱に当たった。{} m 先", *t);
}
```

## 関数

### KR::MakeOBB()　箱を作る
```cpp
OBB MakeOBB(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& halfSize,
            DirectX::FXMMATRIX rotation);
```
回転行列から、`OBB` を作る。行列の 1 行目が右(X)、2 行目が上(Y)、3 行目が前(Z)の軸になる(DirectXMath の決まり)。描画のワールド行列と同じ**回転**を渡せば、見た目と判定の向きがそろう。`rotation` には、移動・拡大を含めない。

| 引数 | 説明 |
|---|---|
| `center` | 中心(世界の座標。メートル) |
| `halfSize` | 各軸の方向に、中心から面までの距離(半分の長さ) |
| `rotation` | 回転の行列(移動・拡大を含めない) |

### KR::Intersects()　重なっているか
```cpp
bool Intersects(const OBB& a, const OBB& b);        // 箱と箱(分離軸定理)
bool Intersects(const Sphere& a, const Sphere& b);  // 球と球
bool Intersects(const Sphere& sphere, const OBB& box);
bool Intersects(const OBB& box, const Sphere& sphere);
```
2 つの形が、重なっているか。ちょうど接するときも、`true`。球が箱の中に入っているときも、`true`。

### KR::Raycast()　光線が当たるか
```cpp
std::optional<float> Raycast(const Ray& ray, const Sphere& sphere);
std::optional<float> Raycast(const Ray& ray, const OBB& box);
```
光線が当たるか。当たれば、**始点から当たった点までの距離 t**(0 以上。`direction` の長さが 1 ならメートル)を返す。外れれば `std::nullopt`。後ろ向き(形が始点の後ろ)は、外れ。始点が形の中なら、`t = 0`。

### KR::RayToSegmentDistance()　光線と線分の距離
```cpp
float RayToSegmentDistance(const Ray& ray, const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b,
                           float* outRayT = nullptr);
```
光線と、線分(`a` から `b`)の最短距離(メートル)。細い物(骨の線・矢印)を、マウスで選ぶのに使う。光線は、始点から前だけ。`outRayT` を渡すと、光線の上で一番近い点までの距離 `t` も返る(手前の物を選ぶ目安)。

### KR::GetCorners()　箱の 8 つの角
```cpp
std::array<DirectX::XMFLOAT3, 8> GetCorners(const OBB& box);
```
`OBB` の 8 つの角。番号は、ビット 0 が右(1 なら +)・ビット 1 が上・ビット 2 が前。箱を線で描く(`DrawWireBox`)ときに使う。

## 列挙型・構造体

### OBB　向きを持てる箱
| メンバー | 既定値 | 説明 |
|---|---|---|
| `center` | (0, 0, 0) | 中心(世界の座標。メートル) |
| `axes[3]` | 右・上・前 | 箱の 3 本の軸。長さ 1・互いに直角 |
| `halfSize` | (0.5, 0.5, 0.5) | 各軸の方向に、中心から面までの距離 |

### Sphere　球
| メンバー | 既定値 | 説明 |
|---|---|---|
| `center` | (0, 0, 0) | 中心(世界の座標。メートル) |
| `radius` | 0.5 | 半径(メートル) |

### Ray　光線
始点から、ある向きに、まっすぐ伸びる線。`Camera::ScreenPointToRay()` で作れる。

| メンバー | 既定値 | 説明 |
|---|---|---|
| `origin` | (0, 0, 0) | 始点(世界の座標。メートル) |
| `direction` | (0, 0, 1) | 向き(長さ 1) |

## アクセサー一覧
なし(構造体のメンバーを、直接読み書きする)。
