# 敵の行動(Behavior)

ノードで作った行動のグラフ(状態機械 = FSM)を、保存して、読んで、毎フレーム「今の状態」を決める部分。
ノードエディタ(`KRBehaviorEditor`)と、動いている敵の「今のノード」を光らせる表示は、あとの段階で足す(下の「進み具合」)。

名前空間は `KR::Behavior`。公開ヘッダー = `include/KR/Behavior/`、非公開 = `src/Behavior/`。

## 1. 何を作るか、なぜ、エンジンの仕事か
- Action2D の敵の行動(待機・移動・攻撃)を、**コードを書き換えずに、ノードで作る**ため。エディタは、エンジンの仕事(Ryusei の決定)。ゲームは、できたデータを読んで使うだけ。
- **エンジンは、物理・移動・当たり判定を持たない**(ゲーム固有)。エンジンは「今、どの状態か」だけを返し、ゲームが、それを見て動く。ゲーム固有の型は、エンジンに入れず、汎用の値の入れ物(`Context`)で渡す。
- 調査の結論は、Action2D の `docs/EnemyAI_Research.md`。2D アクションの定番は FSM なので、まず FSM。
  - **選ばなかった案**:ビヘイビアツリー(BT)は、今回は作らない。あとで足せるよう、`Graph` に `graphType`("fsm")を持たせてある。

## 2. データの形(`Graph.h`)
| 型 | 中身 |
|---|---|
| `ParamValue` | `std::variant<float, int, bool, std::string>`。ノードの値 1 つ |
| `ParamMap` | 名前 → `ParamValue`(`std::map`。JSON に書いたとき、順番がいつも同じになる) |
| `Node` | `id`・`kind`(State / Condition)・`type`("Wait" など)・位置・`params` |
| `Link` | `from → to` と `priority` |
| `Graph` | `version`・`graphType`・`startNode`・`nodes`・`links`。`FindNode(id)`・`OutgoingLinks(id)` |
| `GraphPtr` | `shared_ptr<const Graph>`。読み込んだあとは変えず、複数の敵で共有する |

- **意味は「状態 → 条件 → 状態」**。条件は、状態と状態の間にあるノード。同じ状態から複数の条件が出ているとき、`priority` の小さい順に調べる(`OutgoingLinks` が、その順で返す。同じ priority は、links の並びのまま)。
- つながっていない状態は、その状態のまま。
- **選ばなかった案**:線に条件を持たせる(線 = 条件)。依頼が「条件は、ノードで、状態の間に置く」だったので、条件をノードにした。条件にも値(半径など)があり、エディタで同じ欄の仕組みを使えるのも、利点。
- id は、グラフの中で重ならない。消した番号は使い回さない(エディタの仕事。B4-4)。読み込みでは、重なっていたら壊れたファイルとして扱う。

## 3. ノードの種類の登録(`NodeTypes.h`)
- 種類ごとに `NodeTypeInfo { kind, type, 日本語の名前, params[ParamSpec] }` を登録する。`ParamSpec` は、名前・日本語の名前・既定値・最小・最大。**値の型は、既定値の型で決まる**。
- **なぜ登録か**:エディタが、登録から値の欄を自動で作る。種類を足すときは、`NodeTypes.cpp` の `MakeBuiltinTypes` に 1 つ書くだけ。
- 組み込みは、最初に使われたときに自動で入る(`Registry()` の初期化)。**選ばなかった案**:`RegisterBuiltinNodeTypes()` を、ゲームが呼ぶ形。呼び忘れが起きるのと、静的ライブラリでは、静的登録がリンクで落とされることがあるため。
- 重複した type・空の type は、登録しない(`false`)。

| 種類 | type | 値(既定) |
|---|---|---|
| 状態 | `Wait` | なし |
| | `Patrol` | speed 1.5 |
| | `MoveToPlayer` | speed 2.5 |
| | `Attack` | boxWidth 1.0・boxHeight 0.5・damage 1(int)・windupSeconds 0.4・activeSeconds 0.2・recoverySeconds 0.4 |
| 条件 | `PlayerWithin` | key "distanceToPlayerX"・radius 3.0・includeVertical false・keyY "distanceToPlayerY" |
| | `PlayerOutside` | 同上(radius 6.0) |
| | `TimeElapsed` | seconds 1.0 |
| | `AttackFinished` | key "attackFinished" |

既定値(特に radius と Attack の数字)は、仮の値。記録なし(依頼に指定がなかったため、実装担当が仮に決めた)。ゲームに合わせて、直してよい。

## 4. 保存と読み込み(`GraphFile.h`)
- `SaveGraph(graph, file)`:JSON(版 1)に書く。一時ファイル(`.tmp`)に書いてから置き換える = **書いている途中のファイルを、読まれない**(あとの自動再読み込みのため)。float は、いちばん短い表記(0.2 は 0.2)で書く。
- `LoadGraphFile(file, out)` → `Loaded` / `NotFound` / `Invalid`。`LoadGraph(file)` はゲーム向けで、**いつも null でないグラフ**を返す(読めなければ、エラーを出して、待機 1 つのグラフ)。
- JSON のキーは、アルファベット順に並ぶ(nlohmann/json の標準)。読み書きには影響せず、順番がいつも同じになる。**選ばなかった案**:`start` を先頭に固定する順(ordered_json)。差分が読みやすくなるが、今は必要性が小さい。
- nlohmann/json は、エンジンの中だけ(PRIVATE)。公開ヘッダーに出さない。

### 壊れたファイルの扱い(落ちない)
| 壊れ方 | 動き |
|---|---|
| ファイル無し | エラー + 待機 1 つのグラフ |
| JSON として壊れている | エラー + 待機 1 つ |
| 版が違う | エラー + 待機 1 つ(読まない) |
| 未知のノードの種類 | エラー + 待機 1 つ(ファイル全体を捨てる) |
| ノード id の重なり / 状態が 1 つも無い | エラー + 待機 1 つ |
| 値が欠けた / 型が違う | その値だけ既定値 |
| `links` が無い | 線なしとして読む |
| 開始ノードが無い・状態でない | 警告 + 最初の状態から始める |
| 存在しないノードへの線 | 警告 + その線だけ捨てる |

- **選ばなかった案**:未知の種類のノードだけを飛ばす。線が宙に浮いて、グラフの意味が変わるので、ファイル全体を捨てて安全側にした。

## 5. 状況の入れ物(`Context.h`)
- ゲームが毎フレーム、`SetFloat` / `SetInt` / `SetBool` で入れる(キー = 文字列)。`GetFloat` / `GetInt` / `GetBool(key, 既定値)`・`Has`・`Clear`。キーが無い・型が違うときは、既定値。`GetFloat` は int の値も float として読む(ゲームが `SetInt` で入れても、距離の条件が読めるように)。

## 6. 条件の評価(`src/Behavior/Condition.cpp`。非公開)
- `EvaluateCondition(node, context, timeInState)`。
- `PlayerWithin` / `PlayerOutside`:**横(X)だけの距離**が基本。`includeVertical` が true のときは、縦(`keyY`)も含めて √(x²+y²) で比べる(Ryusei の決定)。**ちょうど半径は、内側**(`距離 <= 半径` が内)。
- キーが無いときは、判定できないので、`PlayerOutside` も偽(「内でない」を「外」とはしない)。
- `TimeElapsed`・`AttackFinished`:次の段階(B2-3)。

## 7. 進み具合(B1〜B6)
済:B1-1〜B1-8(データ・登録・保存・読み込み)・B2-1(Context)・B2-2(距離の条件)。
これから:B2-3〜(条件の残り・Runner)→ B3(imnodes)→ B4(エディタ)→ B5(デバッグ表示・再読み込み)→ B6(サンプル・docs・版 0.4.0)。タスクの表は `Shared/handoff/KREngine-B-ノード担当.md`。
使い方(`Usage.md` の 8)は、ゲームが呼べる形(Runner)ができてから書く(B6-3)。
