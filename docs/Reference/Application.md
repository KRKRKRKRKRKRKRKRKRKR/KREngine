---
title: Application(ゲームの本体)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Application

## 概要
ゲームの本体になるクラス。ウィンドウ・描画・音・入力・メインループを、エンジンが用意する。ゲームは、`KR::Application` を**継承**して、`OnInit`・`OnUpdate`・`OnRender`・`OnImGui`・`OnShutdown` のうち、必要な関数を書く。`main()` は、エンジンが持っているので、ゲームは書かない。代わりに、`KR::CreateApplication()` を 1 つ書く。

流れ:`OnInit` → ループ { `OnUpdate` → `OnRender` → `OnImGui` } → `OnShutdown`

ヘッダー:`KR/Core/Application.h`(`#include <KR/KR.h>` で使える)

## サンプルコード
キューブを回す、最小のゲーム。

```cpp
#include <KR/KR.h>
#include <memory>

using namespace DirectX;

class MyGame : public KR::Application
{
public:
    // ウィンドウの題名・大きさを渡す(省略すると "KR Engine"・1280 × 720)
    MyGame() : Application(KR::ApplicationDesc{ .title = "MyGame", .width = 1280, .height = 720 }) {}

protected:
    void OnInit() override
    {
        m_Cube = GetRenderer().CreateMesh(KR::CreateCubeMeshData());   // 最初に 1 回:形を作る
    }
    void OnUpdate(float deltaTime) override
    {
        m_Angle += deltaTime;                                           // 毎フレーム:動かす(deltaTime = 前のフレームからの秒数)
    }
    void OnRender() override
    {
        GetRenderer().DrawMesh(m_Cube, XMMatrixRotationY(m_Angle));    // 毎フレーム:描く依頼を積む
    }
    void OnShutdown() override
    {
        GetRenderer().DestroyMesh(m_Cube);                              // 最後に 1 回:片付ける
    }

private:
    KR::MeshHandle m_Cube;
    float m_Angle = 0.0f;
};

// エンジンの main() が呼ぶ。作ったゲームを返す(これだけ書けば、main は、いらない)
std::unique_ptr<KR::Application> KR::CreateApplication() { return std::make_unique<MyGame>(); }
```

## 関数

### Application::Application()　ウィンドウを作る
```cpp
explicit Application(const ApplicationDesc& desc = {});
```
ウィンドウ・描画・音を準備する。ゲームのクラスのコンストラクターから、`Application(KR::ApplicationDesc{ ... })` として呼ぶ。

| 引数 | 説明 |
|---|---|
| `desc` | ウィンドウの題名・大きさなど(下の `ApplicationDesc`)。省略すると、既定値 |

### Application::OnInit()　最初に 1 回
```cpp
virtual void OnInit();
```
起動した直後に、1 回だけ呼ばれる。形・画像・音・モデルの読み込みは、ここで行う。

### Application::OnUpdate()　毎フレーム:動かす
```cpp
virtual void OnUpdate(float deltaTime);
```
毎フレーム呼ばれる。入力を読む・位置を動かす・カメラや光を `Renderer` に渡す、といった処理を書く。

| 引数 | 説明 |
|---|---|
| `deltaTime` | 前のフレームからの時間(秒)。動かす量に掛けると、どのモニターでも、同じ速さになる |

### Application::OnRender()　毎フレーム:描く
```cpp
virtual void OnRender();
```
毎フレーム、`OnUpdate` の後に呼ばれる。`DrawMesh` などで、描く依頼を積む。実際の描画は、フレームの最後に、まとめて行われる。

### Application::OnImGui()　デバッグの窓
```cpp
virtual void OnImGui();
```
`OnRender` の後に呼ばれる。`ImGui::Begin(...)` から `ImGui::End()` の間に、デバッグの窓を書く。ゲームの絵より、手前に乗る。`#include <imgui.h>` が要る。

### Application::OnShutdown()　最後に 1 回
```cpp
virtual void OnShutdown();
```
終了する直前に、1 回だけ呼ばれる。番号札(形・画像)や、`AnimatedModel` を、返す。

### Application::GetRenderer()　描画の窓口を取る
```cpp
Renderer& GetRenderer();
```
描画の窓口(`Renderer`)を返す。派生クラスの中で使う。→ [Renderer](Renderer.md)

### Application::GetAudio()　音の窓口を取る
```cpp
Audio& GetAudio();
```
音を鳴らす窓口(`Audio`)を返す。→ [Audio](Audio.md)

### Application::Run()　ループを回す
```cpp
void Run();
```
メインループを回す。エンジンの `main()` が呼ぶので、**ゲームは呼ばない**。

### KR::CreateApplication()　ゲームを返す
```cpp
std::unique_ptr<Application> CreateApplication();
```
**ゲームが 1 つだけ実装する**関数。エンジンの `main()` が呼ぶ。自分のクラスを `std::make_unique` で作って、返す。

## 列挙型・構造体

### ApplicationDesc　ウィンドウの設定
`Application` のコンストラクターに渡す設定。`KR::ApplicationDesc{ .title = "..." }` のように、必要な項目だけ書く。

| メンバー | 既定値 | 説明 |
|---|---|---|
| `title` | `"KR Engine"` | ウィンドウの題名 |
| `width` | 1280 | ウィンドウの幅(画素) |
| `height` | 720 | ウィンドウの高さ(画素) |
| `enableEditorKey` | `true` | Debug で F1 を押すと、敵の行動のツールを開く。ツール自身は `false` にする |
| `pauseGameWhileEditing` | `true` | F1 でツールを開いたとき、ゲームを止めるか。`false` なら、動かしたまま直せる(リアルタイム編集) |

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `GetRenderer()` | 描画の窓口を返す |
| `GetAudio()` | 音の窓口を返す |
