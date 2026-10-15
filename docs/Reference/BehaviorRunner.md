---
title: BehaviorRunner(敵の今の状態を決める)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# BehaviorRunner

## 概要
敵の行動を、**ノードのグラフ**で作って、ゲームで動かす仕組み(`KR::Behavior`)の、中心になる部分。グラフは、ツール `KRBehaviorEditor` で作る(状態 → もし [条件] なら → 次の状態)。ゲームは、毎フレーム、次の 3 つのことをする。

1. **状況を入れる**:`Context` に、ゲームの今の様子(プレイヤーとの距離・攻撃が終わったか、など)を、**キーと値**で入れる。
2. **`Runner::Update()`**:`Runner` が、状況を見て、敵の「今の状態」を決める。
3. **今の状態を見て、動く**:`runner.GetCurrentState()` が返す `StateView` から、状態の種類(`"Patrol"` など)と値(`speed` など)を読んで、**ゲームが**動く。

**エンジンは、物理・移動・当たり判定を持たない**。「どの状態か」を決めるだけで、実際に歩く・攻撃する処理は、ゲームが書く。

- `Runner` は、**敵 1 体ごとに 1 つ**。グラフ(`GraphPtr`)は、同じ種類の敵で共有してよい。
- 1 回の `Update()` で、状態が移るのは、**1 回まで**(状態が、1 フレームで、通り過ぎない)。
- グラフの作り方・読み込みは → [BehaviorGraph](BehaviorGraph.md)。デバッグの表示は → [BehaviorDebug](BehaviorDebug.md)。

ヘッダー:`KR/Behavior/Runner.h`・`Context.h`

## サンプルコード
左右に巡回して、プレイヤーが近づいたら追いかけて、攻撃する敵。

```cpp
// ① 起動時:グラフを読む(敵の種類ごとに 1 回)
KR::Behavior::GraphPtr graph = KR::Behavior::LoadGraph(KR::GetAssetDirectory("Enemies") / "Walker.json");

// ② 敵ごとに Runner を 1 つ(作った直後は、開始の状態に入っている)
KR::Behavior::Runner runner(graph);

// ③ 毎フレーム
KR::Behavior::Context context;
context.SetFloat("distanceToPlayerX", std::abs(playerX - enemyX));   // 条件ノードの key と、名前をそろえる
context.SetBool("attackFinished", attackFinished);
runner.Update(deltaTime, context);

const KR::Behavior::StateView state = runner.GetCurrentState();     // Update のたびに、取り直す
if (state.GetType() == "Patrol")
{
    enemyX += direction * state.GetFloat("speed") * deltaTime;       // 値は、グラフに書いた物(ツールで直せる)
}
else if (state.GetType() == "MoveToPlayer")
{
    // プレイヤーへ近づく(state.GetFloat("speed"))
}
else if (state.GetType() == "Attack")
{
    if (state.EnteredThisFrame()) { attackTime = 0.0f; }             // 入った最初のフレームだけ true
    attackTime += deltaTime;
    attackFinished = attackTime >= state.GetFloat("windupSeconds")
                                 + state.GetFloat("activeSeconds")
                                 + state.GetFloat("recoverySeconds");
}
// "Wait"(待機)は、何もしない
```

## 関数

### Runner::Runner()　敵 1 体ぶんを作る
```cpp
explicit Runner(GraphPtr graph);
```
開始ノードの状態に入った状態で作る。開始ノードが正しくなければ、最初の状態から。状態が 1 つも無い(または `nullptr` の)グラフなら、待機 1 つの既定のグラフを使う。

### Runner::Update()　1 フレーム進める
```cpp
void Update(float deltaTime, const Context& context);
```
時間を進めて、今の状態の**遷移の行**を、上から調べて、最初に成り立った行の、次の状態へ移る。移ったら、入ってからの時間は 0 に戻り、`EnteredThisFrame()` が `true` になる(次の `Update` で `false`)。

| 引数 | 説明 |
|---|---|
| `deltaTime` | 前のフレームからの秒 |
| `context` | ゲームの今の状況 |

### Runner::GetCurrentState()　今の状態を見る
```cpp
StateView GetCurrentState() const;
```
今の状態を読む窓口(`StateView`)を返す。`Update()`・`SetGraph()` のたびに、取り直す。

### Runner::SetGraph()　グラフを差し替える
```cpp
void SetGraph(GraphPtr graph);
```
グラフを差し替える(ファイルの再読み込み用)。今の状態の id が、新しいグラフにも状態として残っていれば、その状態と時間を引き継ぐ。無ければ、新しいグラフの開始の状態に戻る。

### Runner::Reset()　開始の状態に戻す
```cpp
void Reset();
```
今のグラフの開始の状態に戻す(時間は 0・`EnteredThisFrame()` は `true`)。

### Runner::GetLastConditionResults()　条件の結果を見る
```cpp
const std::vector<ConditionResult>& GetLastConditionResults() const;
```
直前の `Update()` で、実際に評価した条件のノードと、その結果(デバッグの表示用。`DrawGraphDebug` が、真は緑・偽は赤で出す)。最初に成り立った行から先は、評価しない。

### Context::SetFloat() / SetInt() / SetBool()　状況を入れる
```cpp
void SetFloat(const std::string& key, float value);
void SetInt(const std::string& key, int value);
void SetBool(const std::string& key, bool value);
```
ゲームの今の状況を、キー(文字列)と値で入れる。例:`SetFloat("distanceToPlayerX", 2.5f)`・`SetBool("attackFinished", true)`。エンジンは、ゲーム固有の型を知らない。条件ノードが、**キー**で値を読む。キーが無い条件は、偽になり、警告が 1 回、ログに出る。

### Context::GetFloat() / GetInt() / GetBool()　状況を読む
```cpp
float GetFloat(const std::string& key, float defaultValue = 0.0f) const;
int   GetInt(const std::string& key, int defaultValue = 0) const;
bool  GetBool(const std::string& key, bool defaultValue = false) const;
```
キーが無い・型が違うときは、`defaultValue` を返す(`GetFloat` は、整数の値も、小数として読む)。

### Context::Has()　キーがあるか
```cpp
bool Has(const std::string& key) const;
```
そのキーに、小数・整数・真偽のどれかの値が入っているか。

### Context::Clear()　空にする
```cpp
void Clear();
```
入れた状況を、全部消す。

### StateView::GetType()　状態の種類
```cpp
const std::string& GetType() const;
```
状態の種類(`"Wait"`・`"Patrol"`・`"MoveToPlayer"`・`"Attack"` など)。

### StateView::GetTimeInState()　入ってからの時間
```cpp
float GetTimeInState() const;
```
この状態に入ってからの秒。

### StateView::EnteredThisFrame()　入った最初のフレームか
```cpp
bool EnteredThisFrame() const;
```
この状態に入った `Update` の直後か。入った**最初のフレームだけ** `true`。攻撃の開始の合図などに使う。

### StateView::GetFloat() / GetInt() / GetBool() / GetString()　状態の値を読む
```cpp
float GetFloat(const std::string& name, float defaultValue = 0.0f) const;
int   GetInt(const std::string& name, int defaultValue = 0) const;
bool  GetBool(const std::string& name, bool defaultValue = false) const;
std::string GetString(const std::string& name, const std::string& defaultValue = {}) const;
```
状態の値(`speed` など。グラフに書いてある物)。無い・型が違うときは、`defaultValue`。

## 列挙型・構造体

### 内蔵の状態の種類
| `GetType()` | 値(既定値) | ゲームがすること |
|---|---|---|
| `Wait`(待機) | なし | 動かない |
| `Patrol`(巡回) | `speed`(1.5) | 左右に歩いて、壁で反転する(壁の判定は、ゲーム) |
| `MoveToPlayer`(プレイヤーへ移動) | `speed`(2.5) | プレイヤーへ近づく |
| `Attack`(攻撃) | `boxWidth`(1.0)・`boxHeight`(0.5)・`damage`(1)・`windupSeconds`(0.4)・`activeSeconds`(0.2)・`recoverySeconds`(0.4) | 溜め → 出ている(箱で、当たり判定)→ 硬直。**秒は、ゲームが数える** |

### 内蔵の条件の種類
| 条件 | 値(既定値) | 使う `Context` のキー |
|---|---|---|
| `PlayerWithin`(プレイヤーが半径以内) | `key`("distanceToPlayerX")・`radius`(3.0)・`includeVertical`(false)・`keyY`("distanceToPlayerY") | `key`(縦も含めるなら `keyY` も) |
| `PlayerOutside`(半径の外) | 同上(`radius` 6.0) | 同上 |
| `TimeElapsed`(秒たった) | `seconds`(1.0) | なし(エンジンが数える) |
| `AttackFinished`(攻撃が終わった) | `key`("attackFinished") | `key` |
| `And`(かつ)・`Or`(または)・`Not`(でない) | 入力の条件(つなぐ) | なし |

距離は、横(X)だけが基本。`includeVertical` が `true` のときは、縦も含めて、√(x² + y²) で比べる。ちょうど半径は、内側。

### ConditionResult　条件の結果
| メンバー | 説明 |
|---|---|
| `node` | 条件のノードの id |
| `value` | 結果(真 / 偽) |

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `Runner::GetGraph()` | 今のグラフを返す |
| `Runner::GetCurrentState()` | 今の状態を返す |
| `StateView::GetNodeId()` | 今の状態のノードの id |
| `StateView::GetType()` | 状態の種類 |
| `StateView::GetTimeInState()` | 入ってからの秒 |
