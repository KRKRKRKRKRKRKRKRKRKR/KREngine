---
title: 更新の記録(CHANGELOG)
project: KREngine
kind: 記録
status: 最新
updated: 2026-10-19
replaces:
---

# 更新の記録(CHANGELOG)

KR エンジンの、版ごとの変更。版の付け方・上げるときの決まりは [docs/Versioning.md](docs/Versioning.md)。
新しい版を出すたびに、一番上に足す。**ゲーム側で、直す必要がある変更は、「ゲーム側で必要な変更」に、必ず書く**。

書き方:`追加`(新しい機能)・`変更`(今までの動きが変わった)・`修正`(バグの修正)・`ゲーム側で必要な変更`。

## [未リリース]
(次の版に入る変更を、ここに、書いていく。版を出すときに、下へ移す)

## [0.18.0] — 2026-10-22(ブランチ feature/unlit-material。merge・タグは、まだ)
光を受けない材質。画面に固定して出す UI の物(ゲージ・文字)を、夜の暗い光に関係なく、決めた色のまま出す。
- 追加:`Material::unlit`(既定 false)。true なら、光・ハイライト・トゥーン・フォグを飛ばして、物の色 × 画像 × `diffuseColor` を、そのまま画面の色にする。材質の `b3` の `params.w` で渡す(大きさは、変わらない)
- ゲーム側で必要な変更:**なし**(`unlit` の既定は false = 今までの見た目)

## [0.17.0] — 2026-10-22(ブランチ feature/toon。merge・タグは、まだ)
トゥーン(アニメ調)。光の当たり方を、段階の帯に切る。**輪郭線・影は、まだ無い**。
- 追加:`Material::toonSteps`(帯の数。0 = なめらか = 既定。2〜16)・`Material::toonSoftness`(帯の境目のなめらかさ。0〜0.5。既定 0.05)。太陽・点光源・スポットライトの、拡散(当たり具合)を、光ごとに帯に切る。ハイライトは、しきい値のある、はっきりした点になる(`specularStrength` を 0 より大きくしたとき)
- 内部:材質の `b3` の `params` の y・z を使う(大きさは、変わらない)。`shaders/Common.hlsli` に `Toon`・`ToonSpecular`
- ゲーム側で必要な変更:**なし**(`toonSteps` の既定は 0 = 今までの見た目。基準画像との差 0 で確かめた)

## [0.16.0] — 2026-10-21(ブランチ feature/lighting。merge・タグは、まだ)
ライティングの強化(機能の追加)。**点光源・スポットライト・ハイライト**。影・法線マップ・トゥーンは、まだ無い。
- 追加:**点光源** `KR::PointLight`(位置・色・届く距離)・**スポットライト** `KR::SpotLight`(位置・向き・色・届く距離・内側と外側の角度)。`Lighting` の `pointLights`(最大 16)・`spotLights`(最大 8)に足す。明るさは、距離の 2 乗に反比例 × 届く距離で滑らかに 0。超えた分・使えない光は、警告を 1 回出して使わない
- 追加:**材質** `KR::Material`(拡散の掛け色・ハイライトの色・鋭さ・強さ)。`Renderer::DrawMesh(mesh, world, material, texture, overlay)`・`DrawSkinnedMesh(..., palette, material, texture)`・`AnimatedModel::Draw(renderer, world, material, texture)` の、材質つきの形を足した。ハイライトは Blinn-Phong で、太陽・点光源・スポットライトの全部に出る(`specularStrength` の既定は 0 = 出ない)
- 追加:**`CreateSphereMeshData(rings, segments, color)`**(球。直径 1 メートル)
- 追加:**`Renderer::DrawLightGizmo(PointLight / SpotLight)`・`DrawLightGizmos(Lighting)`**(光の範囲を、手前に描く線で。Release では何もしない)
- 内部:ルートシグネチャに `b3`(材質。ピクセルシェーダー専用の root constants 12 個)を足した。`FrameConstants` に光の配列を足した(`static_assert` で並びを確かめる)。`shaders/Common.hlsli` に `BlinnPhong`・`Attenuation`・`SpotFactor`
- ゲーム側で必要な変更:**なし**。光も材質も、渡さなければ、今までと同じ見た目(基準画像との差を数えて確かめた)。ただし、**`DrawMesh(mesh, world, {})` のように、画像の位置に `{}` を書いていると、材質つきの形と区別できず、コンパイルエラーになる**(`TextureHandle{}` と書く)。すれすれ飛行・Action2D には、そのような書き方は無かった

## [0.15.0] — 2026-10-19
ゲームから、ゲームを終わらせる(機能の追加)。
- 追加:**`Application::RequestQuit()`**(派生クラスの中で呼ぶ)。窓の × を押したときと同じ道筋で終わる(このフレームは最後まで進み、次のフレームの前にループが終わって、`OnShutdown` が呼ばれる)。タイトルの「終了」などに使う。中身は、非公開の `Window::RequestClose()`(`WM_CLOSE` を送るだけ)
- 追加:**リファレンスを Zip で配る** `tools\Build-Reference.ps1 -Zip` → `dist\KREngine-Reference-v<版>.zip`(`reference.html`・`README.txt`・`THIRD_PARTY_LICENSES.txt`)。表示の部品(marked 4.3.0・highlight.js 11.9.0・Mermaid 10.9.1)を HTML の中に入れるので、**インターネットが無くても見られる**。部品は版を固定し、SHA256 で確かめる(`build\reference-libs\` にためる)。説明の元は `docs/Reference/_zip/`
- ゲーム側で必要な変更:**なし**(終わらせたいときだけ、`RequestQuit()` を呼ぶ)

## [0.14.0] — 2026-10-15
ゲームの CMake を、短くする(機能の追加)。ゲームの `CMakeLists.txt` を、**3 行ほど**にできる。
- 追加:**`kr_add_game(<名前> [SOURCES <フォルダー>...] [LIBRARIES <ライブラリ>...])`**。`add_executable`・`#include` の場所(`src`)・エンジンへのリンク・DLL とアセットのコピー・Release の設定・Visual Studio のスタートアップを、まとめて行う。**ソースは、フォルダーの下(既定 `src`)の `.cpp`・`.h`・`.hpp` を、自動で集める** = ファイルを足しても、`CMakeLists.txt` を直さなくてよい(足した後の最初のビルドで、CMake が自動で設定し直す。**そのビルドは、古いプロジェクトで進むので、新しいファイルは、2 回目のビルドで入る**)。使い方は `docs/Usage.md` の 2-2
- 変更:`kr_copy_runtime_files` が、アセットのフォルダーの一覧を、自動で取り直す(`CONFIGURE_DEPENDS`)。**`Assets/` の下に、フォルダー(`Enemies` など)を足したときに、CMake を、もう一度 configure しなくてよい**(次のビルドで、自動で設定し直して、その次のビルドで、コピーされる)
- 修正:ゲームが `KR_GAME_ASSETS_DIR` を決めていないとき、エンジンが決めた既定値(ゲームの `Assets/`)が、ゲーム側の CMake から見えず、`kr_copy_runtime_files` が、アセットをコピーしなかった(`PARENT_SCOPE` で、ゲーム側にも渡す)。今までのゲームは、自分で決めていたので、影響なし
- 追加(文書):**リファレンス**(`docs/Reference/`。機能ごとのページ 20 + 目次。概要・サンプルコード・関数・構造体・アクセサー一覧・実行画面)・`docs/QuickReference.md`(はじめての人向け)・`tools/Build-Reference.ps1`(リファレンスを 1 つの Web ページにする)
- ゲーム側で必要な変更:**なし**(0.13.0 までの書き方は、そのまま動く。`kr_add_game` は、後から、使い始められる)

## [0.13.0] — 2026-10-15
ノードエディタの、拡大縮小・複数条件(かつ・または・でない のネスト)・遷移の行ごとのピン(UE5 のブランチ風)。Action2D からの依頼。
- 追加:**拡大縮小**(ホイール)。ノードエディタの部品を imnodes から imgui-node-editor に替えた(エンジンの中だけ = ゲームの CMake は変わらない)
- 追加:**複数条件・ネスト**:条件 `And`(かつ)・`Or`(または)・`Not`(でない)。入力に、条件や別の かつ/または を入れられる(循環は作れない)
- 追加:**遷移の行ごとの別のピン**:状態ノードに「もし [条件] なら → [次の状態]」の行を並べる(上から調べる)。ツールで、行の並べ替え・追加・削除
- 変更:グラフの JSON を **版 2** に(`transitions`・`inputs`)。**版 1 のファイルは、読むときに自動で変換**(Action2D の Walker・Chaser・Guard とサンプルで、状態の列が変わらないことを確認)。ツールで保存すると、版 2 になる
- 変更:`Graph::links` をやめた(ゲームが直接使っていなければ、影響なし)。`LoadGraph`・`Runner`・`Context`・`GraphAsset`・`DrawGraphDebug`・`ObserveRunner` の呼び方は、同じ
- 変更:`DrawGraphDebug` が、遷移の行・かつ/または・条件の真偽の色(緑 / 赤)を描く。`Runner::GetLastConditionResults()` を追加
- 追加:検証の警告(未完成の行・論理の入力不足・使われていない条件・循環・たどり着けない状態)
- ゲーム側で必要な変更:**版 2 で保存したファイルは、古いエンジン(0.12.0 まで)では読めない**。ツール(新しいエンジンのビルド)で `Assets/Enemies/*.json` を保存し直したら、**ゲームのサブモジュール `External/KREngine` を、この版に進める**こと(進めなければ、ゲームは、エラー + 待機だけのグラフになる)。版 1 のまま、保存し直さないなら、古いエンジンでも新しいエンジンでも動く。`Graph::links` を直接読むコードがあれば、`transitions` に直す

## [0.12.0] — 2026-10-14
ツールのグラフの上で、動いている敵の今の状態を光らせる(機能の追加。Debug だけ。Action2D からの依頼)。
- 追加:**ツールのグラフの上で、動いている敵の今の状態を光らせる**(リアルタイムの観測。Debug だけ。Action2D からの依頼)。ゲームが、見てほしい敵を毎フレーム `KR::Behavior::ObserveRunner(runner, asset)`(新しい公開ヘッダー `KR/Behavior/Observe.h`)で知らせると、エンジンが約 0.1 秒おきに観測ファイル(`%TEMP%\KREngineLive\…\observe.json`)に書き、ツールが読んで、そのノードを橙(止まっていれば灰色)・出る線を橙に光らせる。違うグラフなら「<名前> を開く」ボタン。ゲームが閉じたら「つながっていません」
- 追加:`GraphEditor::SetObservedNode(node, paused)`(光らせるノード)
- 追加:サンプル KRBehavior に `ObserveRunner` の 1 行
- ゲーム側で必要な変更:**なし**(呼ばなければ、今と同じ。一度も呼ばないゲームは、観測ファイルも書かない)。使うときは、`OnUpdate` の中で `ObserveRunner` を毎フレーム呼ぶ(`docs/BehaviorEditor.md` の「動いている敵の今の状態を光らせる」)

## [0.11.0] — 2026-10-14
リアルタイム編集(機能の追加。Debug だけ。Action2D からの依頼)。
- 追加:**リアルタイム編集**(Action2D からの依頼)。ツール KRBehaviorEditor で直すと、**保存しなくても**、約 0.2〜0.7 秒で、動いているゲームの敵に反映する(Debug だけ)。ツールが「ライブファイル」(`%TEMP%\KREngineLive\…`)に書き、`GraphAsset` がそれも見る。本物のファイルは、保存したときだけ書き換わる。保存せずに閉じる・別のグラフを開く・ツールが落ちる → ゲームは保存の内容に戻る。`Runner::SetGraph` の状態の引き継ぎは、そのまま。新しい公開ヘッダー `KR/Behavior/LiveGraph.h`(ツールとエンジン用。ゲームは呼ばなくてよい)
- 追加:`ApplicationDesc::pauseGameWhileEditing`(既定 true = 今と同じ)。F1 で開いたときに、ゲームを止めるかの初期値。右上の小さな窓の ☑ で、今までどおり切り替えられる
- 修正:`tools/AnimationEditor/EditorApp.cpp` の C4456(変数名の重複。ゲームの /W4 で出ていた)。`GraphAsset`・`GraphFile` のログの場所を UTF-8 に(日本語のフォルダーで文字化けしていた)
- ゲーム側で必要な変更:**なし**(既定のままなら、今と同じ)。F1 で開いても止めないなら、`ApplicationDesc` に `.pauseGameWhileEditing = false`

## [0.10.0] — 2026-10-14
ゲームから F1 で、敵の行動のツールを開く(機能の追加。Debug だけ)。
- 追加:**Debug のゲームで F1 → 敵の行動のツール(KRBehaviorEditor)を開く**。ゲームの Assets フォルダーを渡して起動し、もう開いていれば前に出す。開いている間、ゲームは一時停止(`OnUpdate` を呼ばない)。右上の小さな窓「エディター(F1)」の ☑ で、動かしたままにもできる。ツールを閉じると再開。ツールで保存すると、ゲームが読み直す(`GraphAsset` を使っているゲーム)
- 追加:`ApplicationDesc::enableEditorKey`(既定 true)。ツールの 2 つは false(F1 で自分を開かない)
- 変更:`kr_copy_runtime_files` が、ツール `KRBehaviorEditor` があれば、**Debug のときだけ** exe の横にコピーし、ゲームと一緒にビルドする(`add_dependencies`)。エンジンの CMake で、サンプルをツールの後に追加するようにした
- ゲーム側で必要な変更:なし(F1 を使わないなら)。**F1 を使うなら**、ゲームの CMake で、エンジンを取り込む前に `set(KR_BUILD_TOOLS ON CACHE BOOL "" FORCE)`(無ければ、F1 で警告のログが出るだけ)。Release のゲームには、F1 の機能もツールも入らない

## [0.9.0] — 2026-10-14
MP3 の読み込み(機能の追加)。
- 追加:**MP3 の読み込み**(`Audio::Load` が、WAV・OGG に加えて、MP3 も読める)。ライブラリは **dr_mp3**(取り込み済みの dr_libs の同じコミット。0.7.4。新しい取り込みは無い)。形式は、先頭の "ID3" か、MP3 のフレームの印で判断する。**ループさせる BGM は、今までどおり OGG を勧める**(MP3 は、継ぎ目にすき間が出やすい)
- ゲーム側で必要な変更:なし(追加だけ)

## [0.8.0] — 2026-10-14
音(機能の追加)。XAudio2 で、効果音と BGM を鳴らす(最低限の機能)。
- 追加:**音(`KR::Audio`。`KR/Audio/Audio.h`)**。XAudio2 で鳴らす。`Application::GetAudio()` で使う。`Load`(ゲームの `Assets/Sounds/` からの場所。WAV・OGG を全部メモリに読む)・`Unload`・`Play`(ループ・音量。同じ音を重ねて鳴らせる)・`Stop`・`SetMasterVolume` / `GetMasterVolume`。札は `SoundHandle`(読んだ音)・`VoiceHandle`(鳴っている音)。鳴り終わった音は、エンジンが毎フレーム片付ける。音の機器が無くても落ちない(警告 + 音なし)
- 追加:外部ライブラリ **dr_wav**(`mackron/dr_libs` のコミット `dfe8377`。WAV を読む)。OGG は、取り込み済みの stb の **stb_vorbis**。どちらもエンジンの中だけ(PRIVATE)。リンクに `xaudio2`・`ole32`
- 追加:サンプル **KRAudio**(`samples/Audio/`。Space = 効果音・B = BGM・上下 = 全体の音量)と、音 `samples/Assets/Sounds/`(プログラムで作った `Beep.wav`・Kenney の CC0 の `Jingle.ogg`)
- 無いもの(最低限の版):一時停止・BGM と効果音で別々の音量・フェード・ピッチ・3D の音・ストリーミング(`docs/Audio.md` の「既知の制限」)
- ゲーム側で必要な変更:なし(追加だけ)。音を使うときは、`Assets/Sounds/` を作って、CMake を、もう一度 configure する(Release で exe の横へコピーする一覧に入れるため)

## [0.7.0] — 2026-10-14
入力の拡張(機能の追加)。キーボードの全部のキーと、マウス(ボタン・位置・ホイール)。
- 追加:**キー `Key::O`・`Key::P`**(Action2D の攻撃・パリィ。右手で押すキー)(このあと、全部のキーを足したので、下の項目に含まれる)
- 追加:**キーボードの全部のキー**(A〜Z・上の段の 0〜9 `Num0`〜`Num9`・F1〜F12・矢印・Enter Space Escape Tab Backspace・Shift Ctrl Alt(左右を区別しない)・Insert Delete Home End PageUp PageDown・テンキー `Numpad0`〜`9`・`NumpadAdd` `NumpadSubtract` `NumpadMultiply` `NumpadDivide` `NumpadDecimal` `NumpadEnter`)。記号のキー(; , . / など)は、入れていない(日本語の配列と英語の配列で、位置と意味が違うため)
- 追加:**マウスのボタンを、キーと同じ `Key` に入れた**(`Key::MouseLeft` `MouseRight` `MouseMiddle` `MouseX1` `MouseX2`)。`IsKeyDown` / `IsKeyPressed` / `IsKeyReleased` が、そのまま使える
- 追加:**マウスの位置 `Input::GetMousePosition()`**(ウィンドウの中のピクセル。`Camera::ScreenPointToRay` に、そのまま渡せる)と、**ホイール `Input::GetMouseWheel()`**(このフレームの目盛りの数。奥へ回すと +)
- 追加:`Key::Count`(キーの数)と `Input::GetKeyName(Key)`(キーの名前。全部のキーを順に調べる・表示するときに使う)
- 変更:ImGui の窓の上では、マウスの「押した」とホイールをゲームに渡さない(キーボードと同じ扱い。「離した」と位置は、いつも渡す)
- 変更:**Alt(または F10)だけを押して離しても、Windows の「メニューを選ぶ状態」に入らない**ようにした(入ると、メインループが止まり、次のキーが Windows のメニューに取られていた)。Alt+Space(ウィンドウのメニュー)・Alt+F4 は、今までどおり
- 変更:`Key` の並び(列挙の数値)が変わった。名前(`Key::W` など)は、今までのものを全部残してある
- サンプル KRMinimal:ImGui の窓に、マウスの位置・ホイールの合計・押しているキーを出し、押した・離したキーをログに出す
- ゲーム側で必要な変更:なし(名前は変わっていない)。ただし、`Key` を**数値として**ファイルに保存しているゲームは、読み直しが必要(並びが変わったため。名前で保存するなら `GetKeyName` を使う)

## [0.6.0] — 2026-10-12
FBX の骨の拡大の修正(バグの修正)。版は、仲介担当が決める。
- 修正:**FBX の骨の世界の行列に、100 倍の拡大が残っていた**(Blender の FBX は、アーマチュアに 100 倍の拡大が付く)。そのため、骨に付けた判定の形が、100 倍の大きさで出た。骨の行列(バインド・動き)から、共通の拡大を取り除くようにした(スキニングの結果は、変わらない)。glTF の骨は、拡大 1
- 追加:サンプルのモデル `samples/Assets/Models/Test/HumanModel/`(`.glb` と `.fbx`。人型・骨 212 本・動き「Walk」)。起動時の確認ログに、HumanModel の glTF と FBX の比較を足した
- 変更:**骨の上限を、128 本から 512 本に増やした**(`Renderer::MaxSkinJoints`・シェーダーの `MaxJoints`)。HumanModel(212 本)が、描けなかったため。定数バッファの上限(64 KB)により、1024 本が、限度。glTF も FBX も同じ
- ゲーム側で必要な変更:なし

## [0.5.0] — 2026-10-12
モデルの読み込みに **FBX(`.fbx`)** を追加(機能の追加)。
- 追加:**FBX の読み込み**(`LoadFbx`・`Animation/FbxLoader.h`。ライブラリ **ufbx** v0.23.1。エンジンの中だけ・公開ヘッダーに型を出さない)。メッシュ(三角形・法線・UV)・骨・スキンの重み(1 頂点 4 本まで)・動き(アニメーションスタック → 1 秒 30 回の Linear に焼き付け)を読む。マテリアル・画像・モーフは、読まない。座標は、エンジン(左手系・Y が上・メートル)に直す
- 追加:**`LoadModel`**(`Animation/LoadModel.h`。拡張子で、`.glb` / `.gltf` → `LoadGltf`・`.fbx` → `LoadFbx` を選ぶ)。**`AnimatedModel::Load` が使う**ので、ゲームは、`Load("Boss/Boss.fbx")` と書くだけで、FBX も動く
- 変更:**`GltfModel` を `ModelData` に改名**(`LoadGltf` / `LoadFbx` / `LoadModel` が返す型。意味は同じ)
- 追加:調整ツールのモデル一覧に `.fbx`(同名の `.glb` と `.fbx` は、`.hitboxes.json` を共有する)。外部ライブラリ ufbx は `third_party/ufbx/`(ufbx だけ、Debug でも最適化)
- 追加:FBX で、骨とメッシュが離れているとき、読み込み時に警告を出す(Blender の置き場所の違い。直し方は `docs/Usage.md` の 7-1)
- **ゲーム側で必要な変更:`GltfModel` を直接使っているコードがあれば、`ModelData` に直す**(ゲームは、普通 `AnimatedModel` だけを使うので、影響は、ほぼ無い。**古い版の `samples/Skeleton` を持つゲームのサブモジュールを、進めるときに、そのサンプルが `GltfModel` を使っていれば、直す**)。`AnimatedModel::Load` の呼び出しは、変わらない
- 既知の制限:FBX の 5 本以上の骨が付いた頂点の処理(4 本に絞る)は、実データで未確認・FBX の動きは、30 回/秒に焼き付けるので、元の曲線とは、わずかに違うことがある・**`BossObstacle.fbx` の「基準姿勢の位置」の確認は、書き出し直した FBX 待ち**([Animation.md](docs/Animation.md))

## [0.4.0] — 2026-10-12
敵の行動を、ノードで作る仕組みの追加(機能の追加)。
敵の行動を、ノードで作って、ゲームで動かす仕組み(`KR::Behavior`)と、ツール。
- 追加:**`KR::Behavior`**(`Behavior/`)。ゲームから使えるのは、公開側 `include/KR/Behavior/` の、次の物(`Graph`・`Context`・`Runner`・`GraphFile`・`GraphAsset`・`GraphDebug` は、`KR.h` から、まとめて使える)
  - `Graph`・`Node`・`Link`(グラフの型。状態 → 条件 → 状態)・`NodeTypes`(ノードの種類の登録。エディタの欄は、登録から自動で作られる)
  - `LoadGraph` / `SaveGraph`(JSON。版つき。壊れたファイル・欠けた項目でも、落ちない)・`Context`(状況の入れ物)・**`Runner`**(毎フレーム「今の状態」を決める。`StateView` で、種類・値・入ってからの時間を返す)・**`GraphAsset`**(ファイルが書き換わったら、約 0.5 秒おきに見て、自動で読み直す。壊れていたら、前のまま)
  - **`DrawGraphDebug`**(動いている敵の「今の状態」のノードを、ゲームの ImGui の窓の中で、光らせる。読むだけの表示)
  - 状態:`Wait`・`Patrol`・`MoveToPlayer`・`Attack` / 条件:`PlayerWithin`・`PlayerOutside`(横だけの距離が基本。`includeVertical` で、縦も含める)・`TimeElapsed`・`AttackFinished`
  - **エンジンは、物理・移動・当たり判定を持たない**(ゲームが、`Runner` が返す状態を見て、自分で動く)
- 追加:**ツール `KRBehaviorEditor`**(`tools/BehaviorEditor/`)。ノードで、行動のグラフを作る(足す・つなぐ・値を直す・開始ノード・分岐の順番・検証の警告)。開いたゲームの `Assets/Enemies/<名前>.json` に保存する(開く・一覧・保存・新規・読み直し・保存していない印と確認・設定の記憶・`--assets` / `--graph`)
- 追加:**サンプル `KRBehavior`**(`samples/Behavior/`。グラフ `samples/Assets/Enemies/Walker.json`)・外部ライブラリ **imnodes**(MIT。ノードエディタの部品。エンジンの中だけ = PRIVATE。コミットで固定。ズームの機能は、無い)
- 追加:`docs/Behavior.md`(経緯・決めたこと・既知の制限)・`docs/BehaviorEditor.md`・**`docs/Usage.md` の 8(ゲームでの使い方)**
- 変更(ツールの中だけ。ゲームには影響なし):`FolderDialog` を `tools/AnimationEditor/` から `tools/Common/`(静的ライブラリ `KRToolCommon`)へ移し、2 つのツールで共有した
- **ゲーム側で必要な変更:なし**(追加だけ。`KR.h` に、`KR::Behavior` の公開ヘッダーが増えたが、既存の物は変わらない。ゲームの CMake は、そのままでよい)
  - 敵の行動を使うゲーム(Action2D)は、`Usage.md` の 8 のとおりに呼ぶ。**`Enemies` フォルダーを、初めて作ったときは、CMake を、もう一度 configure する**(exe の横へ、コピーする一覧が、configure のときに決まるため)
- 既知の制限:ノードエディタに、ズームが無い(imnodes の機能の制限)・元に戻す / コピーが無い・ツールの窓の閉じるボタンでは、保存の確認が出ない(エンジンの `Application` に、閉じる前の仕組みが無い)

## [0.3.0] — 2026-10-09
調整ツール KRAnimationEditor を、画面の上で、直接操作できるように強化(機能の追加)。
- 追加:**レイ**(`Physics/Ray.h`)・`Camera::ScreenPointToRay`(画面の点 → 世界のレイ)・`Raycast(Ray, Sphere)` / `Raycast(Ray, OBB)`(当たれば距離 t)・`RayToSegmentDistance`(レイと線分の最短距離)
- 追加:**`MirrorHitbox`**(判定の形を、X = 0 で左右反転して、対の骨に付け直す)・`MirrorBoneName` / `FindMirrorJoint`(対の骨の名前。`.L/.R`・`_L/_R`・`Left/Right`・`左/右`)(`Animation/HitboxMirror.h`)
- 追加:調整ツールの、**クリックで、判定の形・骨を選ぶ**・**ギズモ(ImGuizmo)で、形の移動・大きさ**(T = 移動 / S = 大きさ。Blender 準拠)・**コピー / 貼り付け**(Ctrl+C / V)・**左右反転して貼り付け**
- 追加:外部ライブラリ **ImGuizmo**(調整ツールだけ。FetchContent・コミットの番号で固定。エンジン本体・ゲームには入らない)
- ゲーム側で必要な変更:なし(追加だけ。`Collision.h` が `Ray.h` を取り込むようになったが、既存の物は変わらない)

## [0.2.1] — 2026-10-08
版の仕組みを入れた版(機能は、0.2.0 と同じ)。
- 追加:エンジンの版(`project(KREngine VERSION ...)`)・`KR::GetVersion()` / `GetVersionString()`・起動時のログに版を出す・調整ツールのタイトルに版を出す
- 追加:この `CHANGELOG.md`・`docs/Versioning.md`(版の付け方と、上げるときの決まり)・`CLAUDE.md` に、ボーンアニメーションと文字コードの決まり

## [0.2.0] — 2026-10-08
ボーンアニメーションと、調整ツール。
- 追加:**ボーンアニメーション**(`Animation/`)
  - glTF(.glb)の読み込み(cgltf)・骨・動きの再生・GPU スキニング(`Renderer::DrawSkinnedMesh`)
  - 骨に追従する当たり判定の形(箱・球)・`Sphere` と球の判定(`Intersects`)・`Renderer::DrawWireSphere`
  - **`AnimatedModel`**(読み込み・再生・描画・判定の形を、まとめた窓口。ゲームは、これだけ使う)
  - 判定の形の保存(`<モデル名>.hitboxes.json`。nlohmann/json は、エンジンの中だけ)
  - 骨の表示(`BoneRenderer`。Blender のような八面体)
- 追加:**調整ツール `KRAnimationEditor`**(`tools/AnimationEditor/`。骨・判定の形を調整して、開いたゲームのフォルダーに保存する。ゲームは、読み込むだけ)
- 追加:`KR::SetAssetRoot` / `GetAssetRoot`(アセットのフォルダーを、実行時に切り替える。ツール用)・`KR::GetCommandLineArguments`・`DrawMesh` / `DrawLine` の `overlay`(深度テストなしで、手前に描く)
- 追加:サンプル `KRAnimated`(`AnimatedModel` の使い方の見本)
- 修正:**日本語を含むフォルダーに置くと、Debug ビルドが、起動直後に落ちる**問題(`GetAssetDirectory` が、UTF-8 の場所を、CP932 として読んでいた)
- 変更:改行コードを LF にそろえた(`.gitattributes`)
- ゲーム側で必要な変更:なし(`Renderer` の `DrawMesh` / `DrawLine` に、省略できる引数 `overlay` が足されただけ。Root Signature に、骨用の項目が足されたが、エンジンの中)

## [0.1.0] — 2026-10-08
エンジンを、ゲームから切り出して、単体で動く形にした版。
- 追加:エンジンの CMake を、単体で動く形に(`project(KREngine)`・単体のとき C++23 / サンプルのビルド)・最小のサンプル `KRMinimal`
- 追加:エンジンの使い方(`docs/Usage.md`)・エンジンの docs(`docs/`)・サブモジュールで、ゲームに取り込む手順
- 変更:シェーダーを、エンジンの `shaders/` に移した。ゲームのアセットの場所は、`KR_GAME_ASSETS_DIR`(ゲームの CMake が決める)
- 変更:名前を、TenFiftyEight から **KR / KREngine** に変えた(名前空間 `KR`・マクロ `KR_`・ライブラリ `KREngine`)
- ゲーム側で必要な変更:名前空間・マクロ・インクルードのパス(`<KR/...>`)の書き換え。`kr_copy_runtime_files` を呼ぶ。ゲームのアセットの場所を `KR_GAME_ASSETS_DIR` で、エンジンに教える

(これより前の、エンジンの成り立ち(DX12 の土台・OBJ・入力・光・テクスチャ・当たり判定の箱など)は、切り出す前のゲームのリポジトリの履歴と、`docs/` の各ファイルに、残っている)
