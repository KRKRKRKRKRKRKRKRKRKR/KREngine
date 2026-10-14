---
title: 起動から終了・1フレーム
project: KREngine
kind: 設計
status: 最新
updated: 2026-10-14
replaces:
---

# 起動から終了・1フレーム

アプリの起動 → メインループ → 終了の流れと、1フレームの中で CPU / GPU がどう動くか。

[← 資料の案内(Structure.md)に戻る](Structure.md)

## 起動から終了まで

```mermaid
sequenceDiagram
    autonumber
    participant EP as EntryPoint main
    participant Game as GameApp
    participant App as Application
    participant R as Renderer
    participant DX as DX12 部品

    EP->>Game: CreateApplication()
    Game->>App: 親のコンストラクタ(Window 作成)
    App->>R: new Renderer
    R->>DX: Device → Queue → SwapChain → DepthBuffer → CommandList
    R->>DX: シェーダーをコンパイル → Pipeline 作成
    R->>DX: カメラ用の定数バッファ ×2
    R->>DX: SRV ヒープ → ImGui の初期化(Win32 / DX12)
    EP->>App: Run()
    App->>Game: OnInit()
    Game->>R: CreateMesh(6色キューブ・Cube・木・自機・地面)→ 番号札 ×5 / CreateTexture(player.png)
    Note right of Game: チャンクを全部読む(LoadAllChunks)→ ChunkManager::Initialize

    loop 毎フレーム
        App->>Game: OnUpdate(deltaTime)(Debug で F1 のツールが開いていて「ゲームを止める」なら、呼ばない)
        App->>App: Audio::Update()(鳴り終わった音を片付ける)
        Note right of Game: 背景色 / Player::Update(入力を読んで速度・位置)<br/>ChunkManager::Update(前方に足す・後方を捨てる)<br/>SyncChunkMeshes(チャンクごとの形を、作る・捨てる)<br/>追従カメラの位置を決めて SetCamera
        App->>R: BeginFrame()
        App->>Game: OnRender()
        App->>Game: OnImGui()(Game が ImGui の窓を書く)
        App->>App: DrawEditorPanel()(Debug で F1 のツールが開いている間だけ。右上の小さな窓)
        Game->>R: DrawMesh × 約 20(回るキューブ・OBJ のキューブ・木・自機(画像つき)・地面・チャンク 約 12 個。描画リストに積むだけ)
        App->>R: EndFrame()(描画リストを命令にして提出・表示)
        App->>App: InputState::EndFrame()(押した・離した の付箋を剥がす)
    end

    App->>Game: OnShutdown()
    Game->>R: DestroyMesh(5 つの形 + 置いてあるチャンクの形)
    EP->>R: Renderer のデストラクタ
    R->>DX: Flush → 形・バッファ・部品を逆順に解放 → 解放漏れチェック
```

| 処理 | 場所 |
|---|---|
| Renderer のコンストラクタ | [Renderer.cpp:60](../src/Renderer/Renderer.cpp#L60) |
| メインループ | [Application.cpp:27](../src/Core/Application.cpp#L27) |
| 入力の付箋を剥がす(フレームの最後) | [Application.cpp:52](../src/Core/Application.cpp#L52) |
| F1 でツールを開く・一時停止(Debug) | [Application.cpp](../src/Core/Application.cpp)(`OpenBehaviorEditor`・`Run`)。詳しくは [BehaviorEditor.md](BehaviorEditor.md) の「ゲームから F1 で開く」 |
| Game の初期化(形を作る) | GameApp.cpp:33 |
| Game の更新(自機の動き・追従カメラ) | GameApp.cpp:50 |
| Game の描画(DrawMesh) | GameApp.cpp:92 |
| Game の終了(DestroyMesh) | GameApp.cpp:127 |
| Renderer のデストラクタ | [Renderer.cpp:93](../src/Renderer/Renderer.cpp#L93) |


---

## 1フレームの流れ

```mermaid
sequenceDiagram
    participant Game as GameApp
    participant R as Renderer
    participant L as CommandList
    participant GPU

    Note over Game: OnUpdate
    Game->>R: SetCamera(m_GameCamera)

    Note over R: BeginFrame(最後に ImGui の NewFrame)
    R->>R: 2フレーム前の Fence を待つ
    R->>R: ReleaseFinishedMeshes(使い終わった形を捨てる)
    R->>L: Barrier 表示用 → 描画用
    R->>L: 背景色でクリア / 深度を 1.0 でクリア
    R->>L: 描画先(RTV + DSV)・ビューポート
    R->>R: 縦横比を計算 → ビュー × プロジェクション・光・フォグ・空を、今のフレームの定数バッファ(b1)へ
    R->>L: Root Signature / b1 に定数バッファの住所
    R->>L: 空を描く(空のパイプライン。頂点 3 つ。深度なし。最初に描く)

    Note over Game: OnRender / OnImGui
    Game->>R: DrawMesh(番号札, 行列)× 約 135
    Note right of R: 描画リストに積むだけ

    Note over R: EndFrame
    R->>L: ExecuteDrawList(PSO / 形ごとにバッファ設定 / b0 に行列 / Draw)
    R->>L: 描画先を UNORM の RTV・深度なしに切り替え → ImGui を描く(SRV ヒープを設定して RenderDrawData)
    R->>L: Barrier 描画用 → 表示用 → Close
    R->>GPU: Execute(ここで初めて GPU が描く)
    R->>R: Present → Signal
    R->>R: 捨てる予定の形に引換券の番号を書く
```

| 処理 | 場所 |
|---|---|
| BeginFrame | [Renderer.cpp:171](../src/Renderer/Renderer.cpp#L171) |
| 深度バッファのクリア | [Renderer.cpp:193](../src/Renderer/Renderer.cpp#L193) |
| カメラの行列を定数バッファへ | [Renderer.cpp:211](../src/Renderer/Renderer.cpp#L211) |
| 描画リストの実行 | [Renderer.cpp:285](../src/Renderer/Renderer.cpp#L285) |
| EndFrame | [Renderer.cpp:337](../src/Renderer/Renderer.cpp#L337) |


---

## フレームインフライト2 のタイムライン

```
時間 →
frameIndex:  0            1            0            1
CPU:     [F1 を記録]  [F2 を記録]  [F3 を記録]  [F4 を記録]
          Allocator0   Allocator1   Allocator0   Allocator1
          定数バッファ0  定数バッファ1  定数バッファ0  定数バッファ1
                                     ↑ F1 が GPU で終わったか(Fence)確認してから再利用
GPU:              [F1 を実行]  [F2 を実行]  [F3 を実行]
```

**フレームごとに2つずつ持つもの**:Allocator、カメラの定数バッファ(GPU が読んでいる最中に書き換えないため)

