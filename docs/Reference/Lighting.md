---
title: Lighting(光・空・霧)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Lighting

## 概要
画面の見た目を決める、3 つの設定:**光**(`Lighting`)・**空**(`Sky`)・**霧**(`Fog`)。どれも、ゲームが持って、**毎フレーム**、`Renderer` の `SetLighting()`・`SetSky()`・`SetFog()` で渡す。渡さなければ、初期値で描く。

- **光**:太陽のような「平行な光」(世界のどこでも同じ向き)と、環境光(光が当たらない面の最低限の明るさ)。
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

## 関数
`Lighting`・`Sky`・`Fog` は、**値を入れるだけの構造体**で、関数は持たない。渡す関数は `Renderer` にある。

### Renderer::SetLighting()　光を渡す
```cpp
void SetLighting(const Lighting& lighting);
```
このフレームで使う光(コピーして保持する)。

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
