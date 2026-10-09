# 更新の記録(CHANGELOG)

KR エンジンの、版ごとの変更。版の付け方・上げるときの決まりは [docs/Versioning.md](docs/Versioning.md)。
新しい版を出すたびに、一番上に足す。**ゲーム側で、直す必要がある変更は、「ゲーム側で必要な変更」に、必ず書く**。

書き方:`追加`(新しい機能)・`変更`(今までの動きが変わった)・`修正`(バグの修正)・`ゲーム側で必要な変更`。

## [未リリース]
(次の版に入る変更を、ここに、書いていく。版を出すときに、下へ移す)
- 追加(作成中):`KR::Behavior`(敵の行動のノード)。グラフの型(`Graph`)・ノードの種類の登録(`NodeTypes`)・JSON の保存と読み込み(`SaveGraph` / `LoadGraph`)・状況の入れ物(`Context`)。詳しくは [docs/Behavior.md](docs/Behavior.md)
- 追加:外部ライブラリ imnodes(MIT。ノードエディタの部品)を、エンジンの中だけ(PRIVATE)で取り込んだ。ゲームの CMake は、変えなくてよい

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
