# アニメーションの調整ツール KRAnimationEditor

骨つきのモデル(glTF)の**骨・動き・当たり判定の形**を、見て、調整して、保存するツール。調整した結果(`<モデル名>.hitboxes.json`)は、ゲームが `AnimatedModel` で**読み込むだけ**([Usage.md](Usage.md) の 7)。ゲームに、調整の画面は、要らない。

[← 資料の案内(Structure.md)に戻る](Structure.md)

## 起動
```
cmake --preset vs2026
cmake --build build --config Debug --target KRAnimationEditor
build/tools/AnimationEditor/Debug/KRAnimationEditor.exe [--assets <アセットのフォルダー>] [--model <Models からの相対パス>]
```
- **引数なし**:前回の続き(`KRAnimationEditor.txt` に記憶。exe の隣)。初めては、エンジンの `samples/Assets`。
- `--assets`:ゲームの `Assets` フォルダー(中に `Models/` があるフォルダー)。例:`--assets C:\SchoolProgram\Game\Suresurehikouki\Assets`。
- `--model`:`Models/` からの相対パス。例:`--model Boss/Boss.glb`。省略すると、一覧の最初のモデル。
- Visual Studio では、エンジン単体の `.slnx` を開くと、**KRAnimationEditor が、既定のスタートアップ**(F5 で動く)。

## 画面の見方
| 区画 | 項目 | 役割 |
|---|---|---|
| **Project** | Assets(文字欄 + Browse...) | 開くゲームの `Assets` フォルダー。文字欄に入れて Enter・または Browse... で選ぶ。`Models/` が無いフォルダーは、断られる |
| | Recent | 最近使ったフォルダー(最大 8 個) |
| | Model | `Assets/Models/` の下の `.glb` の一覧。選ぶと開く |
| **View** | Bones / Bones in front / Mesh / Rotate model | 骨の表示・骨をメッシュの手前に描く・メッシュの表示・モデルの自動回転 |
| | Reset camera | カメラを、最初の位置に戻す |
| | **Bone** | 骨を、名前の一覧から選ぶ(階層で字下げ)。選んだ骨が橙色になり、下の判定の形の調整の対象になる |
| **Playback** | Clip | 動きを、名前の一覧から選ぶ |
| | Pause / Loop / Speed / Time | 一時停止・ループ・速さ・時刻(止めて、動かすと、姿勢を確かめられる) |
| | Bind pose (no animation) | オンで、動きを止めて、基準姿勢にする。オフで、選んでいる動きを、最初から再生 |
| **Hitboxes** | Show hitboxes | 判定の形(緑の箱・球。選んでいる形は黄・テストの箱に触れた形は赤)の表示 |
| | Test box position | 水色の「テストの箱」の位置。判定の形にぶつけて、当たりを確かめる |
| | Shape / Center / Half size(Radius) | 選んだ骨の形の、種類・中心・大きさ(骨の中の座標。骨が動いても、数字は変わらない) |
| | Add / Remove hitbox | 選んだ骨に、形を足す・消す(1 本の骨に、複数の形も付けられる) |
| | **Save hitboxes** / Reload hitboxes | 保存・読み直し |

**マウス**:左ドラッグ = 回す / 中ドラッグ = 平行移動 / ホイール = 近づく・離れる(ImGui の窓の上では、動かさない)。

## 保存先と、ゲームとの関係
- 保存先は、**開いているアセットのフォルダー**の `Models/` の下(モデルの隣)。エンジンの `KR::SetAssetRoot` で、読み書きの場所を切り替えているので、別のゲームのフォルダーを開いて、そのゲームのフォルダーに保存できる。Debug でも Release でも、同じ。
- ゲーム側は、`AnimatedModel::Load("Boss/Boss.glb")` で、同じ場所の `Boss.hitboxes.json` を読む。ファイルが無ければ、骨ごとの、とりあえずの形になる。
- 骨は、**名前**で探す(無ければ番号)。Blender で、骨の名前を変えたら、保存し直す。

## 未保存の変更
形を変えると、「* Unsaved hitbox changes」が出る。保存せずに、別のモデルやフォルダーに切り替えようとすると、確認の窓が出る(Save and continue / Discard and continue / Cancel)。

## まだ、できないこと(次の段階)
- ビューポートで、形を、直接つかんで動かす(今は、数字で動かす)・クリックで選ぶ
- 元に戻す(Undo)
- 骨そのものの編集(骨は、Blender で作る)
- 動きの、ブレンド・イベント
