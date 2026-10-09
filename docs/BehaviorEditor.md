# 敵の行動の調整ツール KRBehaviorEditor

敵の行動のグラフ(`docs/Behavior.md`)を、ノードで作る・直すツール。`tools/BehaviorEditor/`。前例は `KRAnimationEditor`(`docs/AnimationEditor.md`)。
**今は作成中**(B4-7 まで。保存・開くは、次の段階)。

## 今できること(B4-7)
- 起動すると、見本のグラフ(`MakeStarterGraph()`:待機 → [半径 3 以内] → 移動 → [半径 1 以内] → 攻撃 → [攻撃が終わった] → 待機)が出る。
- 操作は、`GraphEditor`(部品)のとおり(`docs/Behavior.md` の 9)。
  - 何もない所で右クリック = ノードを足す(登録された種類の一覧)/ ノードの上で右クリック = 「ここから始める」・「このノードを消す」
  - ピンからピンへドラッグ = 線(状態 ⇄ 条件だけ。条件は、入力 1・出力 1 まで)/ 線やノードを選んで Delete = 消す
  - 値の欄で、数値・チェック・文字を直す / 状態から出る線が 2 本以上のとき、ノードの中の ▲▼ で、調べる順を変える
- 上に、操作の説明の 1 行と、検証の警告の一覧(警告なし / 警告 N 件)が出る。
- 窓のタイトルに、エンジンの版が出る(`KR Behavior Editor (KR Engine 0.2.1)`)。

## つくり
- 窓の中身は、`GraphEditor`(エンジンの中。imnodes を使う)に、任せる。ツールは、`KREngine` にリンクするだけ(imnodes の型は見えない)。
- 画面いっぱいの 1 枚の窓(タイトルバー・移動・大きさ変更なし)。メニュー・一覧などは、次の段階で、足す。
- ビルド:`KR_BUILD_TOOLS` が ON のとき(エンジン単体のビルドでは、既定で ON)。`KRBehaviorEditor` が、一緒にビルドされる。Visual Studio の既定のスタートアップは、変えていない(`KRAnimationEditor` のまま)。

## まだ、ないこと
- アセットのフォルダーを開く・`Assets/Enemies/*.json` の一覧・保存・新規・読み直し(B4-8〜B4-10)
- 「保存していない」の印・切り替えの確認・設定の記憶・コマンドラインの引数(B4-10)
- `FolderDialog` を、`KRAnimationEditor` と共有(B4-11。エディター強化担当の merge のあと)
- 元に戻す・コピー・ズーム(imnodes に、ズームの機能がない = `docs/Behavior.md` の相性の記録)
