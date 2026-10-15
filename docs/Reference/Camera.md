---
title: Camera(カメラ)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Camera

## 概要
どこから・どこを・どれくらいの範囲で見るかを決めるクラス。長さの単位はメートル。ゲームが `Camera` を持ち、**毎フレーム** `Renderer::SetCamera()` で渡す。渡さなければ、初期値(2 メートル上・5 メートル手前から、原点を見る)で描く。

画面の点(マウスの位置)から、世界へ伸びる光線(`Ray`)を作る `ScreenPointToRay()` もある。3D の物を、マウスで選ぶときに使う。

ヘッダー:`KR/Renderer/Camera.h`

## サンプルコード
プレイヤーを、少し後ろ上から追いかけるカメラ。マウスのクリックで、箱を選ぶ。

```cpp
void OnUpdate(float dt) override
{
    // 追いかけるカメラ(毎フレーム、位置を決めて、Renderer に渡す)
    KR::Camera camera;
    camera.SetPosition({ m_PlayerPos.x, m_PlayerPos.y + 3.0f, m_PlayerPos.z - 6.0f });
    camera.LookAt(m_PlayerPos);
    camera.SetFieldOfView(60.0f);
    GetRenderer().SetCamera(camera);

    // マウスで選ぶ:クリックした場所から、世界へ伸びる光線を作って、箱に当てる
    if (KR::Input::IsKeyPressed(KR::Key::MouseLeft))
    {
        const KR::MousePosition m = KR::Input::GetMousePosition();
        const KR::Ray ray = camera.ScreenPointToRay(m.x, m.y, 1280.0f, 720.0f);   // 幅・高さは、描いている画面の大きさ
        if (const std::optional<float> t = KR::Raycast(ray, m_TargetBox))
        {
            KR_LOG_INFO("箱に当たった。距離 {} m", *t);
        }
    }
}
```

## 関数

### Camera::SetPosition()　カメラの位置
```cpp
void SetPosition(const DirectX::XMFLOAT3& position);
```
カメラを置く位置(世界の座標。メートル)。

### Camera::LookAt()　見る先
```cpp
void LookAt(const DirectX::XMFLOAT3& target);
```
カメラが向く先の点(世界の座標)。

### Camera::SetFieldOfView()　視野
```cpp
void SetFieldOfView(float degrees);
```
上下に見える角度(度)。既定は 60。大きいほど広く見える。

### Camera::SetRollDegrees()　傾き
```cpp
void SetRollDegrees(float degrees);
```
視線の向きを軸にした、カメラの傾き(度)。0 で水平。向きは、動かして確認する(逆なら、符号を反対にする)。

### Camera::SetClipPlanes()　見える距離
```cpp
void SetClipPlanes(float nearZ, float farZ);
```
これより近い物(`nearZ`)・遠い物(`farZ`)は、映らない。既定は、0.1 メートルと 1000 メートル。

### Camera::GetViewMatrix()　ビュー行列
```cpp
DirectX::XMMATRIX GetViewMatrix() const;
```
世界の座標 → カメラから見た座標(カメラが原点、カメラの正面が +Z)の行列。普通は、エンジンが使う。

### Camera::GetProjectionMatrix()　プロジェクション行列
```cpp
DirectX::XMMATRIX GetProjectionMatrix(float aspectRatio) const;
```
カメラから見た座標 → 画面の座標(遠近感と縦横比の補正)の行列。

| 引数 | 説明 |
|---|---|
| `aspectRatio` | 縦横比(幅 ÷ 高さ) |

### Camera::ScreenPointToRay()　画面の点から光線を作る
```cpp
Ray ScreenPointToRay(float x, float y, float width, float height) const;
```
画面の点(左上が (0, 0)・単位は画素)から、世界へ伸びる光線を作る。始点は、カメラの前の面(near)の上の点で、向きは、長さ 1。マウスで、3D の物を選ぶのに使う。

| 引数 | 説明 |
|---|---|
| `x`, `y` | 画面の点(画素)。`Input::GetMousePosition()` の値を、そのまま渡せる |
| `width`, `height` | 描いている画面(ビューポート)の大きさ(画素) |

| 戻り値 | 説明 |
|---|---|
| `Ray` | 光線。`Raycast()` で、球や箱に当てる → [Collision](Collision.md) |

## 列挙型・構造体
なし。

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `GetPosition()` | カメラの位置を返す |
| `GetTarget()` | カメラが見ている点を返す |
| `GetViewMatrix()` | ビュー行列を返す |
| `GetProjectionMatrix()` | プロジェクション行列を返す |
