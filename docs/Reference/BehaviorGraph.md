---
title: BehaviorGraph(グラフの読み込み・保存・自動の読み直し)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# BehaviorGraph

## 概要
敵の行動の**グラフ**(`Graph`)を、ファイルから読む・保存する・書き換わったら読み直す部分。グラフは、**JSON ファイル**(`Assets/Enemies/<名前>.json`。今は `"version": 2`)で、ツール `KRBehaviorEditor` が作る。ゲームは、`LoadGraph()` か `GraphAsset` で読む。

- **壊れたファイル・欠けた項目でも、落ちない**:エラーを出して、安全な既定のグラフ(待機 1 つ)にする。
- **版 1 のファイルは、読むときに、自動で版 2 に変換**する(動きは変わらない)。**版 2 のファイルは、古いエンジン(0.12.0 まで)では読めない**。
- グラフは、読み込んだあとは**変えない**(`GraphPtr` で、複数の敵が共有する)。

ヘッダー:`KR/Behavior/Graph.h`・`GraphFile.h`・`GraphAsset.h`・`NodeTypes.h`

## サンプルコード
グラフを読む。ファイルを書き換えたら、動いているゲームに、すぐ反映する(再起動なし)。

```cpp
// 起動時
KR::Behavior::GraphAsset asset;
asset.Load(KR::GetAssetDirectory("Enemies") / "Walker.json");   // 読めなくても、待機 1 つのグラフになる
KR::Behavior::Runner runner(asset.GetGraph());
uint32_t runnerVersion = asset.GetVersion();

// 毎フレーム(ツールで保存すると、約 0.5 秒以内に、グラフが差し替わる)
asset.ReloadIfChanged(deltaTime);
if (asset.GetVersion() != runnerVersion)
{
    runner.SetGraph(asset.GetGraph());      // 今の状態を引き継ぐ
    runnerVersion = asset.GetVersion();
}
```

## 関数

### KR::Behavior::LoadGraph()　グラフを読む(ゲーム向け)
```cpp
GraphPtr LoadGraph(const std::filesystem::path& file);
```
JSON ファイルから、グラフを読む。読めなければ、エラーを出して、安全な既定のグラフ(待機 1 つ)を返す。**いつも `nullptr` ではない**グラフを返す。

| 引数 | 説明 |
|---|---|
| `file` | フルパス(例 `KR::GetAssetDirectory("Enemies") / "Walker.json"`) |

### KR::Behavior::LoadGraphFile()　グラフを読む(結果が要るとき)
```cpp
GraphLoadResult LoadGraphFile(const std::filesystem::path& file, Graph& out);
```
JSON ファイルから、グラフを `out` に読む。結果が `Loaded`(読めた)・`NotFound`(ファイルが無い)・`Invalid`(壊れている。エラーを出した)のどれかで返る。ツールが使う。

### KR::Behavior::SaveGraph()　グラフを保存する
```cpp
bool SaveGraph(const Graph& graph, const std::filesystem::path& file);
```
グラフを、JSON ファイルに保存する。フォルダーが無ければ作る。一時ファイルに書いてから置き換える(書いている途中のファイルを、読まれない)。失敗したら、エラーを出して `false`。普通は、ツールが呼ぶ。

### KR::Behavior::MakeDefaultGraph()　既定のグラフ
```cpp
GraphPtr MakeDefaultGraph();
```
安全な既定のグラフ(待機 1 つ)。読めなかったファイルの代わりに使われる。

### KR::Behavior::MakeStarterGraph()　見本のグラフ
```cpp
Graph MakeStarterGraph();
```
見本のグラフ。待機 → [プレイヤーが半径 3 以内] → 移動 → [半径 1 以内] → 攻撃 → [攻撃が終わった] → 待機。エディタのはじめの画面に使う。

### GraphAsset::Load()　ファイルを読んで、持つ
```cpp
bool Load(const std::filesystem::path& file);
```
グラフのファイル 1 つを読んで、持つ。読めなかったら(ファイル無し・壊れている)、エラーを出して、待機 1 つの既定のグラフにして、`false`。読めなかったファイルも、あとで直ったら、`ReloadIfChanged` が読み直す。

### GraphAsset::ReloadIfChanged()　書き換わっていたら読み直す
```cpp
bool ReloadIfChanged(float deltaTime);
```
毎フレーム呼ぶ。約 0.5 秒おきに、ファイルの更新時刻と大きさを見て、変わっていたら読み直す。読み直しに成功したら `true`(`GetGraph()` が新しいグラフに・`GetVersion()` が増える)。**壊れたファイルは、前のグラフのまま**(エラーを出す)。

### GraphAsset::ReloadNow()　今すぐ読み直す
```cpp
bool ReloadNow();
```
待たずに、今すぐ、ファイルを見る。成功したら `true`。

### GraphAsset::SetCheckInterval()　見る間隔
```cpp
void SetCheckInterval(float seconds);
```
何秒おきに、ファイルを見るか(既定 0.5)。

### KR::Behavior::RegisterNodeType()　ノードの種類を登録する
```cpp
bool RegisterNodeType(NodeTypeInfo info);
```
ノードの種類を登録する(エンジンの中の `MakeBuiltinTypes` が、内蔵の種類を登録する)。同じ `type` が既にあるか、`type` が空なら、登録せず `false`。エディタの値の欄・追加メニューは、登録から自動で作られる。**ゲームが呼ぶ関数ではない**(新しい種類を足すのは、エンジン側の作業)。

### KR::Behavior::FindNodeType() / GetNodeTypes()　登録を調べる
```cpp
const NodeTypeInfo* FindNodeType(const std::string& type);
const std::vector<NodeTypeInfo>& GetNodeTypes();
```
`type` の説明を返す(無ければ `nullptr`)/ 登録した順の一覧を返す。

### Graph::FindNode()　ノードを id で探す
```cpp
const Node* FindNode(NodeId id) const;
Node* FindNode(NodeId id);
```
`id` のノードを返す。無ければ `nullptr`。ノードは、並びの番号ではなく、**id** で探す(id は、使い回さない)。

## 列挙型・構造体

### Graph　敵 1 種類の行動のグラフ
| メンバー | 説明 |
|---|---|
| `version` | JSON の版(今は 2) |
| `graphType` | グラフの種類(`"fsm"`) |
| `startNode` | 最初の状態のノード id。0 は未指定 |
| `nextNodeId` | 次に発行するノードの id(消しても、使い回さない) |
| `nodes` | ノードの一覧 |

### Node　ノード 1 つ
| メンバー | 説明 |
|---|---|
| `id` | ノードの番号(グラフの中で重ならない) |
| `kind` | `State`(状態)か `Condition`(条件) |
| `type` | 種類の名前(`"Wait"`・`"PlayerWithin"`・`"And"` など) |
| `posX`, `posY` | エディタ上の位置(動きには関係しない) |
| `params` | 値の一覧(名前 → 値) |
| `transitions` | 状態だけ:**遷移の行**。並びの順 = 調べる順 |
| `inputs` | かつ・または・でないだけ:入力の条件のノード id |

### Transition　遷移の行 1 つ
「もし `condition` が成り立つなら、`to` の状態へ移る」。

| メンバー | 説明 |
|---|---|
| `condition` | 条件のノード id(葉の条件でも、かつ・または・でない でもよい)。0 は、つながっていない |
| `to` | 次の状態のノード id |

### ParamValue　値
`std::variant<float, int, bool, std::string>`。速さ = 小数・ダメージ = 整数・縦も含める = 真偽・キー名 = 文字。

### NodeKind
`State`(状態。敵がしていること)・`Condition`(条件。成り立つかを返す部品)。

### NodeTypeInfo / ParamSpec　ノードの種類の説明
| メンバー | 説明 |
|---|---|
| `NodeTypeInfo::type`, `displayName` | 種類の名前・画面に出す日本語の名前 |
| `NodeTypeInfo::params` | 値の説明(`ParamSpec`) |
| `NodeTypeInfo::minInputs`, `maxInputs` | かつ・または・でないの、入力の数の範囲 |
| `ParamSpec::name`, `displayName` | JSON のキー・画面に出す名前 |
| `ParamSpec::defaultValue` | 既定値(型が、値の型を決める) |
| `ParamSpec::minValue`, `maxValue` | 範囲(同じなら、範囲なし) |
| `ParamSpec::visibleWhen` | 空でなければ、その名前の真偽が `true` のときだけ、エディタに出す |

### GraphLoadResult
`Loaded`・`NotFound`・`Invalid`(上の `LoadGraphFile()` を参照)。

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `GraphAsset::GetGraph()` | 今のグラフ(`Load` の前でも `nullptr` ではない) |
| `GraphAsset::GetVersion()` | 読み直しに成功するたびに、1 ずつ増える。ゲームが、「持っているグラフが古いか」を比べる |
| `GraphAsset::GetFile()` | 読んでいるファイル |
