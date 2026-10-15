---
title: BehaviorDebug(グラフの表示・ツールとの連動)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# BehaviorDebug

## 概要
敵の行動のグラフを、**動かしながら確かめる**ための機能。**Debug ビルドだけ**働く物が多い(Release では、何もしないので、`#ifdef` を書かなくてよい)。

- **ゲームの中で見る**(`DrawGraphDebug`):ゲームの ImGui の窓の中に、グラフを描く。動いている敵の「今の状態」のノードが、光る。
- **ツールで見る**(`ObserveRunner`):ゲームが、見てほしい敵を、毎フレーム知らせると、F1 で開いたツール(`KRBehaviorEditor`)のグラフの上で、今の状態が光る。
- **F1 でツールを開く**(Debug):ゲームで F1 を押すと、ツールが開く。開いている間、ゲームを止めるか、動かしたままにするかは、右上の小さな窓で切り替えられる(`ApplicationDesc::pauseGameWhileEditing` が既定)。
- **リアルタイム編集**(Debug):ツールで直すと、約 0.2〜0.7 秒で、動いている敵に反映される(`GraphAsset` が、保存前の「ライブファイル」を先に読む)。

ヘッダー:`KR/Behavior/GraphDebug.h`・`Observe.h`・`LiveGraph.h`

## サンプルコード
ImGui の窓にグラフを描く。見たい敵を、ツールへ知らせる。

```cpp
void OnUpdate(float dt) override
{
    asset.ReloadIfChanged(dt);
    runner.Update(dt, context);
    KR::Behavior::ObserveRunner(runner, asset);        // 見てほしい敵を、毎フレーム知らせる(1 体だけ)
}
void OnImGui() override
{
    ImGui::Begin("敵のグラフ");
    KR::Behavior::DrawGraphDebug(runner);              // 今の状態のノードが光る
    ImGui::End();
}
```

## 関数

### KR::Behavior::DrawGraphDebug()　グラフを描く
```cpp
void DrawGraphDebug(const Runner& runner, float width = 0.0f, float height = 0.0f);
void DrawGraphDebug(const Graph& graph, const Runner& runner, float width = 0.0f, float height = 0.0f);
```
グラフを、ImGui の窓の中に、読むだけで描く(動かせない・編集できない)。動いている敵の今の状態のノードを、橙で光らせる。橙の線は、今の状態から出る線(次に移るかもしれない道)。遷移の行・かつ/または・条件の真偽(緑 / 赤)も描く。`OnImGui` の中で、`ImGui::Begin(...)` と `End()` の間に呼ぶ。大きいグラフは、窓の広さに収める(縮小する。拡大はしない)。

| 引数 | 説明 |
|---|---|
| `runner` | 今の状態を持つ `Runner`。1 つ目の形は、`Runner` が持つグラフを、そのまま描く |
| `graph` | 描くグラフを指定するとき(`GraphPtr` なら `*graph`) |
| `width`, `height` | 描く大きさ。0 なら、窓の残りの広さ |

### KR::Behavior::ObserveRunner()　見てほしい敵を知らせる
```cpp
void ObserveRunner(const Runner& runner, const GraphAsset& asset);
```
見てほしい敵を、**毎フレーム**、`OnUpdate` の中で(`Runner::Update` の後に)知らせる。呼ばなかったフレームは、「見ている敵なし」。敵が倒されて消えたら、呼ぶのをやめるだけでよい。**Release では、何もしない**。グラフの名前は、`asset` のファイル名(拡張子なし。例 `Walker`)。エンジンが、約 0.1 秒おきに、観測ファイルに書き、ツールが読む。

| 引数 | 説明 |
|---|---|
| `runner` | 見てほしい敵の `Runner` |
| `asset` | その敵のグラフを読んだ `GraphAsset`(ファイル名を、ツールへ伝える) |

### KR::Behavior::ReadObservation()　観測ファイルを読む(ツール用)
```cpp
Observation ReadObservation(const std::filesystem::path& enemiesFolder);
```
その `Enemies` フォルダーの観測ファイルを読む。無い・古い(約 1 秒より前)・読めない → `connected = false`。**ツールが使う**。ゲームは呼ばない。

## 列挙型・構造体

### Observation　観測ファイルの中身(ツール用)
| メンバー | 説明 |
|---|---|
| `connected` | ゲームとつながっているか(約 1 秒以内に書かれた) |
| `observing` | ゲームが、敵を見せているか |
| `paused` | ゲームが止まっているか(F1 のツールの「ゲームを止める」) |
| `graphName` | 見ている敵のグラフの名前(例 `Walker`) |
| `nodeId` | 今の状態のノード |
| `stateType` | 今の状態の種類(例 `Patrol`) |
| `timeInState` | 今の状態に入ってからの秒 |

## アクセサー一覧
なし。
