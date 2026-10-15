---
title: Input(入力)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Input

## 概要
キーボードとマウスを読むクラス。すべて `static` 関数なので、`KR::Input::IsKeyDown(KR::Key::W)` のように、どこからでも呼べる。**読むだけ**で、書き込む(押した・離した)のは、エンジンの中だけ。マウスのボタンも、キーと同じ列挙(`KR::Key`)に入っているので、同じ関数で読める。

状態は、毎フレームの最後に更新される。ImGui の窓を使っている間(文字入力・窓の上のクリック)は、「押した」とホイールが、ゲームに届かない。`F10`(シェーダーの再読み込み・Debug)と `F11`(ボーダーレスの切り替え)は、エンジンも使う。

ヘッダー:`KR/Core/Input.h`

## サンプルコード
WASD で動かして、Space でジャンプ、マウスの左クリックで撃つ。

```cpp
void OnUpdate(float dt) override
{
    const float speed = 3.0f;
    if (KR::Input::IsKeyDown(KR::Key::W)) { m_Pos.z += speed * dt; }     // 押している間、毎フレーム
    if (KR::Input::IsKeyDown(KR::Key::S)) { m_Pos.z -= speed * dt; }
    if (KR::Input::IsKeyDown(KR::Key::A)) { m_Pos.x -= speed * dt; }
    if (KR::Input::IsKeyDown(KR::Key::D)) { m_Pos.x += speed * dt; }

    if (KR::Input::IsKeyPressed(KR::Key::Space)) { Jump(); }              // 押した最初のフレームだけ
    if (KR::Input::IsKeyReleased(KR::Key::Space)) { KR_LOG_INFO("離した"); }

    if (KR::Input::IsKeyPressed(KR::Key::MouseLeft))                      // マウスのボタンも、同じ関数
    {
        const KR::MousePosition m = KR::Input::GetMousePosition();
        KR_LOG_INFO("撃った位置: ({}, {})", m.x, m.y);
    }
    m_Zoom += KR::Input::GetMouseWheel();                                 // このフレームのホイール(奥へ回すと +)
}
```

## 関数

### Input::IsKeyDown()　押しているか
```cpp
static bool IsKeyDown(Key key);
```
今、押されているか(押された瞬間のフレームも `true`)。移動など、押している間、続ける動きに使う。

| 引数 | 説明 |
|---|---|
| `key` | 調べるキー(`KR::Key`) |

### Input::IsKeyPressed()　押した瞬間か
```cpp
static bool IsKeyPressed(Key key);
```
このフレームで押されたか。押しっぱなしにしても、**最初の 1 フレームだけ** `true`。ジャンプ・決定のように、1 回だけ起こしたい動きに使う。

### Input::IsKeyReleased()　離した瞬間か
```cpp
static bool IsKeyReleased(Key key);
```
このフレームで離されたか(1 フレームだけ `true`)。

### Input::GetMousePosition()　マウスの位置
```cpp
static MousePosition GetMousePosition();
```
マウスの最後の位置。ウィンドウの描画される部分の左上が (0, 0)で、右が x、下が y(画素)。`Camera::ScreenPointToRay()` に、そのまま渡せる。ボタンを押したまま、ウィンドウの外へ出ると、範囲の外(負の値・幅より大きい値)にもなる。

### Input::GetMouseWheel()　ホイールの量
```cpp
static float GetMouseWheel();
```
このフレームで回したホイールの量(1 = 1 目盛り)。奥へ回すと +、手前へ回すと −。

### Input::GetKeyName()　キーの名前
```cpp
static const char* GetKeyName(Key key);
```
キーの名前("A"・"Space"・"MouseLeft" など)。デバッグの表示や、キーの設定の画面に使う。

## 列挙型・構造体

### Key　キーとマウスのボタン
| 種類 | 値 |
|---|---|
| 文字 | `A`〜`Z` |
| 数字(上の段) | `Num0`〜`Num9` |
| ファンクション | `F1`〜`F12` |
| 矢印 | `Left`・`Right`・`Up`・`Down` |
| よく使うキー | `Enter`・`Space`・`Escape`・`Tab`・`Backspace` |
| 組み合わせ | `Shift`・`Ctrl`・`Alt`(左右を区別しない) |
| 編集 | `Insert`・`Delete`・`Home`・`End`・`PageUp`・`PageDown` |
| テンキー | `Numpad0`〜`Numpad9`・`NumpadAdd`・`NumpadSubtract`・`NumpadMultiply`・`NumpadDivide`・`NumpadDecimal`・`NumpadEnter`(`Enter` とは別。NumLock が ON のとき) |
| マウス | `MouseLeft`・`MouseRight`・`MouseMiddle`・`MouseX1`・`MouseX2`(横の「戻る」「進む」) |
| `Count` | キーの数(キーではない。全部のキーを順に調べるときに使う) |

全部のキーを順に調べる例(サンプル `KRMinimal` と同じ):
```cpp
for (int i = 0; i < static_cast<int>(KR::Key::Count); ++i)
{
    const auto key = static_cast<KR::Key>(i);
    if (KR::Input::IsKeyPressed(key)) { KR_LOG_INFO("押した: {}", KR::Input::GetKeyName(key)); }
}
```

### MousePosition　マウスの位置
| メンバー | 説明 |
|---|---|
| `x` | 横の位置(画素。右が +) |
| `y` | 縦の位置(画素。下が +) |

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `IsKeyDown(key)` | 今、押しているか |
| `IsKeyPressed(key)` | このフレームで押したか |
| `IsKeyReleased(key)` | このフレームで離したか |
| `GetMousePosition()` | マウスの位置 |
| `GetMouseWheel()` | ホイールの量 |
| `GetKeyName(key)` | キーの名前 |
