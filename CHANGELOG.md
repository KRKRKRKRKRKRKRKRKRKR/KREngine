# 更新の記録(CHANGELOG)

KR エンジンの、版ごとの変更。版の付け方・上げるときの決まりは [docs/Versioning.md](docs/Versioning.md)。
新しい版を出すたびに、一番上に足す。**ゲーム側で、直す必要がある変更は、「ゲーム側で必要な変更」に、必ず書く**。

書き方:`追加`(新しい機能)・`変更`(今までの動きが変わった)・`修正`(バグの修正)・`ゲーム側で必要な変更`。

## [未リリース]
モデルの読み込みに **FBX(`.fbx`)** を追加(機能の追加)。版は、merge のときに、仲介担当が決める。
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
