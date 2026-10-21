---
title: Lighting(光・空・霧)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-21
replaces:
---

# Lighting

## 概要
画面の見た目を決める、3 つの設定:**光**(`Lighting`)・**空**(`Sky`)・**霧**(`Fog`)。どれも、ゲームが持って、**毎フレーム**、`Renderer` の `SetLighting()`・`SetSky()`・`SetFog()` で渡す。渡さなければ、初期値で描く。

- **光**:太陽のような「平行な光」(世界のどこでも同じ向き)と、環境光(光が当たらない面の最低限の明るさ)。**0.16.0 から、電球のような点光源(最大 16 個)と、懐中電灯のようなスポットライト(最大 8 個)も置ける**(下の「点光源・スポットライト」)。ハイライト(光の反射のつや)は、物ごとの材質(`Material`)で決める → [Material](Material.md)。
- **空**:地平線が明るく、真上が暗い、グラデーション。画面全体に、最初に描く。
- **霧**:遠くが、霧の色に溶けて見える。霧の色は、空の地平線の色に、自動で合わせる。

色は「物理的な明るさ」の値(ガンマ補正の前)。1.0 を超えてもよい(光の強さ)。

ヘッダー:`KR/Renderer/Lighting.h`

## サンプルコード
夕方の、赤みがかった空と、遠くが霞む景色。

```cpp
void OnUpdate(float dt) override
{
    KR::Lighting light;
    light.toLight = { -0.45f, 0.70f, -0.55f };     // 物から太陽へ向かう向き(長さは、エンジンが 1 にそろえる)
    light.color   = { 1.00f, 0.80f, 0.60f };        // 太陽の光の色(強さ込み)
    light.ambient = { 0.30f, 0.30f, 0.40f };        // 環境光
    GetRenderer().SetLighting(light);

    KR::Sky sky;
    sky.horizonColor = { 0.95f, 0.55f, 0.40f };    // 地平線の色(霧の色も、これに合う)
    sky.zenithColor  = { 0.10f, 0.12f, 0.35f };    // 真上の色
    GetRenderer().SetSky(sky);

    KR::Fog fog;
    fog.density = 0.03f;                            // 大きいほど、近くから霞む
    GetRenderer().SetFog(fog);
}
```

## 点光源・スポットライト(0.16.0 から)
`Lighting` の `pointLights`・`spotLights` に、光を足していく。**空のままなら、今までと同じ見た目**。

```cpp
KR::Lighting light;
light.color   = { 0.2f, 0.2f, 0.3f };   // 太陽は、夜なので、暗く
light.ambient = { 0.05f, 0.05f, 0.08f };

KR::PointLight lamp;                     // 電球
lamp.position = { 3.0f, 1.5f, 10.0f };
lamp.color    = { 8.0f, 6.0f, 4.0f };    // 強さ込み。1 メートル先での明るさ(遠いほど、弱くなる)
lamp.range    = 8.0f;                    // ここまで届く(これより遠くは、全く照らさない)
light.pointLights.push_back(lamp);

KR::SpotLight torch;                     // 懐中電灯
torch.position  = { 0.0f, 3.0f, 0.0f };
torch.direction = { 0.0f, -0.3f, 1.0f }; // 向かう向き(長さは、エンジンが 1 にそろえる)
torch.color     = { 40.0f, 40.0f, 34.0f };
torch.range     = 20.0f;
torch.innerAngleDegrees = 12.0f;         // この内側は、一番明るい(中心からの角度)
torch.outerAngleDegrees = 25.0f;         // ここで 0 になる(内側と外側の間は、滑らかにつながる)
light.spotLights.push_back(torch);

GetRenderer().SetLighting(light);
```
- 明るさは、**距離の 2 乗に反比例**して弱くなり、`range` のところで 0 になる(滑らかに消える)。光の色(強さ)は、1 メートルの距離での明るさなので、遠くまで照らしたいときは、1 を大きく超える値にする。
- 上限(点光源 16・スポット 8)を超えた分は、**警告を 1 回**出して、使わない。使えない光(色が負・位置や向きが数でない・`range` が 0 以下・スポットの向きの長さが 0)も、**警告を 1 回**出して、使わない。スポットの角度が 0〜89 度の外なら、範囲に直す(内側が外側より大きいときは、内側を外側に合わせる)。
- 光の位置と範囲を、画面で確かめるには:`GetRenderer().DrawLightGizmos(light)`(Debug のとき、線で描く。Release では何もしない)。
- 影は、まだ無い(光は、物を通り抜ける)。

## 関数
`Lighting`・`Sky`・`Fog` は、**値を入れるだけの構造体**で、関数は持たない。渡す関数は `Renderer` にある。

### Renderer::SetLighting()　光を渡す
```cpp
void SetLighting(const Lighting& lighting);
```
このフレームで使う光(コピーして保持する)。

### Renderer::DrawLightGizmo() / DrawLightGizmos()　光の範囲を線で描く
```cpp
void DrawLightGizmo(const PointLight& light);
void DrawLightGizmo(const SpotLight& light);
void DrawLightGizmos(const Lighting& lighting);   // 全部の点光源・スポットライト(太陽は、位置が無いので、描かない)
```
光の確認用。点光源は、届く距離の球(3 つの円)・スポットライトは、向きの線・外側の円錐(円と 4 本の線)・内側の円(半分の距離)を、**光の色の線**で、物の手前に描く。`OnRender` で呼ぶ。**Release ビルドでは、何もしない**。

### Renderer::SetSky()　空を渡す
```cpp
void SetSky(const Sky& sky);
```
このフレームで使う空。

### Renderer::SetFog()　霧を渡す
```cpp
void SetFog(const Fog& fog);
```
このフレームで使う霧。

## 列挙型・構造体

### Lighting　光の設定
| メンバー | 既定値 | 説明 |
|---|---|---|
| `toLight` | (−0.45, 0.70, −0.55) | 物から太陽へ向かう向き。面が、こちらを向くほど明るい |
| `color` | (1.00, 0.95, 0.85) | 太陽の光の色(強さ込み。1.0 を超えてもよい) |
| `ambient` | (0.30, 0.33, 0.40) | 環境光。光が当たらない面も、これだけは明るい |
| `pointLights` | 空 | 点光源の並び(`std::vector<PointLight>`)。最大 `MaxPointLights`(16) |
| `spotLights` | 空 | スポットライトの並び(`std::vector<SpotLight>`)。最大 `MaxSpotLights`(8) |

### PointLight　点光源
| メンバー | 既定値 | 説明 |
|---|---|---|
| `position` | (0, 0, 0) | 光の位置(世界の座標・メートル) |
| `color` | (1, 1, 1) | 光の色(強さ込み。1 メートル先での明るさ。負の値は使えない) |
| `range` | 10 | 届く距離(メートル)。0 以下の光は使わない |

### SpotLight　スポットライト
| メンバー | 既定値 | 説明 |
|---|---|---|
| `position` | (0, 0, 0) | 光の位置 |
| `direction` | (0, −1, 0) | 光が向かう向き(長さは、エンジンが 1 にそろえる。長さ 0 は使わない) |
| `color` | (1, 1, 1) | 光の色(強さ込み) |
| `range` | 15 | 届く距離(メートル) |
| `innerAngleDegrees` | 15 | 内側の角度(度)。中心からの角度。この内側は、一番明るい |
| `outerAngleDegrees` | 30 | 外側の角度(度)。ここで 0 になる(0〜89 度) |

### Sky　空の設定
| メンバー | 既定値 | 説明 |
|---|---|---|
| `horizonColor` | (0.85, 0.55, 0.50) | 地平線(視線が水平)の色。霧の色も、これに合う |
| `zenithColor` | (0.15, 0.15, 0.40) | 天頂(真上)の色 |
| `nadirColor` | (0.47, 0.30, 0.28) | 下の色(視線が真下に近いとき。地面が描かれない所に見える) |
| `horizonWidth` | 0.45 | 地平線の光の幅(0〜1)。小さいほど細い帯、大きいほど天頂の近くまで、地平線の色が広がる |

### Fog　霧の設定
| メンバー | 既定値 | 説明 |
|---|---|---|
| `enabled` | `true` | 霧を使うか |
| `density` | 0.02 | 濃さ。見える割合 = exp(−濃さ × 距離)。大きいほど、近くから霞む |

## アクセサー一覧
なし(構造体のメンバーを、直接書き換える)。
