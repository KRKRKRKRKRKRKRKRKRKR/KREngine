---
title: KR エンジン リファレンス(目次)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-21
replaces:
---

# KR エンジン リファレンス

KR エンジンのリファレンスです。以下の機能ごとに分かれています。各ページに、概要・サンプルコード・使う関数(引数・戻り値つき)・構造体・アクセサー一覧が、あります。**初めての人は、先に [はじめてのエンジン(QuickReference)](../QuickReference.md) の「最短で動かす」を読んでください。**

## 基本

| ページ | 内容 |
|---|---|
| [Application](Application.md) | ゲームの本体・メインループ(`OnInit` / `OnUpdate` / `OnRender`) |
| [MyClass](MyClass.md) | 自分のクラス(自機・敵・コイン)の作り方:ファイル・CMake・動きと描画の分け方 |
| [Renderer](Renderer.md) | 描画の窓口(形を描く・線を引く・カメラや光を渡す) |
| [Input](Input.md) | キーボード・マウスを読むクラス |
| [ImGui](ImGui.md) | デバッグの窓(スライダー・ボタン・文字・色・見本の窓) |
| [Log](Log.md) | ログを出すマクロ |
| [Assets](Assets.md) | アセット(モデル・音など)のフォルダーの場所 |
| [Utility](Utility.md) | エンジンの版・起動の引数 |

## エディター(ツールの使い方)

| ページ | 内容 |
|---|---|
| [AnimationEditorGuide](AnimationEditorGuide.md) | 動くモデルの**判定の形**を、画面を見ながら調整するツール `KRAnimationEditor`:起動・操作・通しのチュートリアル(ゲームで当たりを確かめる) |
| [BehaviorEditorGuide](BehaviorEditorGuide.md) | 敵の**行動**を、ノードと線で作るツール `KRBehaviorEditor`:起動・操作・F1 で開く・通しのチュートリアル(敵を動かして、直すとすぐ反映) |

## 見た目

| ページ | 内容 |
|---|---|
| [Mesh](Mesh.md) | 形のデータ(キューブ・板・OBJ・自作) |
| [Texture](Texture.md) | 形に貼る画像 |
| [Camera](Camera.md) | どこから・どこを見るか。画面の点から光線を作る |
| [Lighting](Lighting.md) | 光(太陽・点光源・スポットライト)・空・霧の設定 |
| [Material](Material.md) | 物ごとの質感(ハイライトの強さ・鋭さ・色) |

## 当たり判定

| ページ | 内容 |
|---|---|
| [Collision](Collision.md) | 箱・球・光線の当たり判定 |

## 動くモデル

| ページ | 内容 |
|---|---|
| [AnimatedModel](AnimatedModel.md) | 動くモデル 1 体を、まとめて扱う窓口(読み込み・再生・描画・当たり判定) |
| [Hitbox](Hitbox.md) | 骨にくっついた判定の形 |
| [ModelLoader](ModelLoader.md) | モデル(glTF・FBX)の読み込みの部品・骨・動きのデータ |

## 敵の行動

| ページ | 内容 |
|---|---|
| [BehaviorRunner](BehaviorRunner.md) | 敵の「今の状態」を決める(`Runner`・`Context`・`StateView`) |
| [BehaviorGraph](BehaviorGraph.md) | グラフの読み込み・保存・書き換わったら自動で読み直す |
| [BehaviorDebug](BehaviorDebug.md) | グラフの表示・ツールとの連動(F1・リアルタイム編集・観測) |

## 音

| ページ | 内容 |
|---|---|
| [Audio](Audio.md) | 効果音・BGM を鳴らす |

## ほかの資料
- [はじめてのエンジン(QuickReference)](../QuickReference.md):最短の流れ・関数の早見表・逆引き・よくある間違い
- [使い方(Usage)](../Usage.md):CMake の書き方・Debug と Release・モデル・敵の行動・音の詳しい手順
- [全体の目次(Structure)](../Structure.md)

## Web ページとして見る
この `docs/Reference/*.md` が元の資料です。**1 つの Web ページ**(目次・機能ごとのページ・関数の検索つき)にするには、次を実行します。`docs/Reference/reference.html` ができるので、ダブルクリックで、ブラウザで開けます(インターネットにつながっていると、色つきのコードと、フォントが使えます)。
```
powershell -NoProfile -ExecutionPolicy Bypass -File C:\SchoolProgram\Game\KREngine\tools\Build-Reference.ps1
```
(フルパスで書いてあるので、どのフォルダーの PowerShell から動かしてもよい。`tools\Build-Reference.ps1` だけだと、KREngine のフォルダーにいないときに「存在しません」になる)
`.md` を直したら、もう一度実行します。`reference.html` は、Git には入れません(`.gitignore`)。

## Zip で配る(インターネットが無くても見られる)
ほかの人に渡すときは、`-Zip` を付けて実行します。
```
powershell -NoProfile -ExecutionPolicy Bypass -File C:\SchoolProgram\Game\KREngine\tools\Build-Reference.ps1 -Zip
```
- できる物:`dist\KREngine-Reference-v<版>.zip`(版は `CMakeLists.txt` から自動で取る。例 `v0.14.0`。約 1.2 MB)。中身は `reference.html`・`README.txt`(開き方)・`THIRD_PARTY_LICENSES.txt`(部品のライセンス)。`dist\` は Git には入れない。
- **インターネットが無くても見られる**:ふだんの `reference.html` は、表示に使う 3 つの部品(marked = Markdown の表示・highlight.js = コードの色・Mermaid = 図)をインターネットから読む。`-Zip` では、この 3 つを `reference.html` の中に入れる(HTML は約 3.7 MB になる)。フォントだけは入れない(日本語のフォントは数 MB〜数十 MB と大きいため)ので、インターネットが無いときは、パソコンにあるフォント(游ゴシック・メイリオ・Consolas)で表示される。
- 部品は、**版を固定**して、最初の 1 回だけダウンロードし(`build\reference-libs\`)、**SHA256(ファイルの指紋)で、決めた物と同じかを毎回確かめる**(違えば止まる。CMake の外部ライブラリと同じ考え方)。`_template.html` の部品の版を変えたら、`Build-Reference.ps1` の一覧(URL と SHA256)も直す。
- Zip に入れる説明の元は `docs/Reference/_zip/`(`README.txt`・`THIRD_PARTY_LICENSES.txt`)。
- **決めたこと(2026-10-16。Ryusei の判断)**:部品は HTML の中に入れる(選ばなかった案:Zip の中の別のフォルダー `lib\` に置く = ファイルがばらばらになると見られない/今のまま = インターネットが要る)・中身は HTML と説明だけ(選ばなかった案:元の Markdown と画像も入れる)・置き場所は `dist\`(Publish-Tools.ps1 と同じ)・作り方は `Build-Reference.ps1 -Zip`(選ばなかった案:Publish-Tools.ps1 に入れる)。
- **確かめたこと**:Zip を日本語の名前のフォルダーに展開して、Edge で開いた(画面を撮って見た)→ 目次・コードの色・図(Mermaid)が出た。**ネットワークを止めた Edge** でも、同じに出た。2 回目は、ダウンロードせずに、手元の部品を使った。
