---
title: KREngine(KR エンジン)
project: KREngine
kind: ルール
status: 最新
updated: 2026-10-12
replaces:
---

# KREngine(KR エンジン)

進め方(ペアプログラミングのルール)は、共通。`C:\SchoolProgram\Game\Shared\CLAUDE.md`(`Game\CLAUDE.md` から自動で読まれる)。

DirectX 12 / C++23 / Windows の自作ゲームエンジン。ゲームからは、Git サブモジュール(`External/KREngine`)で使われる。

## このプロジェクト固有のこと
- 資料は `docs/`。目次は `docs/Structure.md`、使い方は `docs/Usage.md`
- ビルド(エンジン単体):`cmake --preset vs2026` → `cmake --build build --config Debug`(サンプル KRMinimal・KRAnimated・ツール KRAnimationEditor が、一緒にビルドされる)。Visual Studio の既定のスタートアップは KRAnimationEditor
- 依存のルール:ゲームが使えるのは公開側 `include/KR/` だけ。`src/` と DX12 の部品は、ゲームから見せない。ゲームは GPU の実体に触らず、番号札(MeshHandle など)だけを持つ
- ゲーム固有のこと(自機・コース・ゲームの設定)は、エンジンに入れない。ゲームのアセットの場所は `KR_GAME_ASSETS_DIR`(ゲームの CMake が決める)
- エンジンを変えたら、`docs/` の該当ファイルも直す。ゲーム側が使うサブモジュールの指す版は、ゲーム側で進める
- コミットメッセージは日本語で、段階の番号を付ける(例:`切り出し E-5: …`)
- **版**:`メジャー.マイナー.パッチ`(決まりは `docs/Versioning.md`)。機能を足す・バグを直す・ゲームのコードを直す必要がある変更をしたら、**版を上げる**(`CMakeLists.txt` の `project(KREngine VERSION ...)` が、唯一の置き場所)・**`CHANGELOG.md` に書く**(ゲーム側で必要な変更は、必ず)・main に merge したコミットに、**タグ `vX.Y.Z` を付ける**。タグの push は、別に頼まれたときだけ
- 作業は、feature ブランチで行い、確認のあとに main へ merge する(push は、頼まれたときだけ)

## ボーンアニメーション(`Animation/`。2026-10-08 に、A-1〜A-8 が完了)
詳しくは `docs/Animation.md`(経過・決めたこと・既知の制限)・`docs/Usage.md` の 7(使い方)・`docs/AnimationEditor.md`(調整ツール)。
- **役割を分ける**:部品(`Skeleton`・`AnimationClip`・`GltfLoader`・`AnimationPose`・`SkinMatrices`・`Hitbox`・`HitboxFile`)= データと計算 / 窓口 = `AnimatedModel`(ゲームは、これだけ使う。部品を、直接呼ばない)/ 描くだけ = `Renderer`(骨つきは `DrawSkinnedMesh`)。Physics(`Collision.h`)は、Animation を知らない
- **glTF は cgltf で読む**(`GltfLoader.cpp` の中だけ。公開ヘッダーに cgltf の型を出さない = ローダーを替えても、ほかは変わらない)。座標系(右手系 → 左手系)の変換は、`GltfLoader.cpp` の先頭の関数に、集めてある
- **FBX は ufbx で読む**(`FbxLoader.cpp` の中だけ。型は、公開ヘッダーに出さない)。`LoadModel`(拡張子で glTF か FBX)を、`AnimatedModel::Load` が呼ぶ。両方が返す型は `ModelData`。ufbx には、右手系で出してもらい、glTF と同じ決まりで Z を反転する(左手系を任せると、この FBX では上下が逆さまになった)。版は固定(v0.23.1)。経過・保留は `docs/Animation.md` の「FBX 対応」
- **判定の形(`Hitbox`)は、骨の中の座標で持つ**。保存は `<モデル名>.hitboxes.json`(nlohmann/json は、エンジンの中だけ = PRIVATE)。骨は**名前**で探す(無ければ番号)
- **調整は、ツール `KRAnimationEditor`(`tools/AnimationEditor/`)で行い、ゲームは、読み込むだけ**。ツールは、開いたゲームの `Assets` フォルダーに保存する(`KR::SetAssetRoot`)。**`SetAssetRoot` は、ツール用。ゲームは、呼ばない**
- 調整ツールの、画面の上の操作(クリックで選ぶ・ギズモで動かす・コピー・左右反転)は、`docs/AnimationEditor.md`。ギズモは、外部ライブラリ ImGuizmo(**ツールだけ**。エンジン本体・ゲームには入れない)。T = 移動 / S = 大きさ(Blender 準拠)
- 使い方の見本は `samples/Animated/`(KRAnimated)。骨つきのメッシュは、1 フレームに 32 個・骨は 128 本まで(`Renderer::MaxSkinnedDrawsPerFrame` / `MaxSkinJoints`)
- 手前に描く(`overlay`。深度テストなし)は、骨つきの形には使えない

## 敵の行動(`Behavior/`。B1〜B6-3 が完了。版・タグは、仲介担当が決める)
詳しくは `docs/Behavior.md`(経緯・決めたこと・既知の制限)・`docs/Usage.md` の 8(使い方)・`docs/BehaviorEditor.md`(ツール)。
- **役割を分ける**:データと計算 = `Graph`(型)・`NodeTypes`(ノードの種類の登録)・`GraphFile`(JSON の読み書き)・`Context`(状況)・`Runner`(今の状態を決める)・`GraphAsset`(ファイルの書き換えを読み直す)/ 画面 = `GraphEditor`(ノードエディタの部品。ツール用)・`DrawGraphDebug`(ゲームの中で、今の状態を光らせる。読むだけ)。**エンジンは、物理・移動・当たり判定を持たない**(ゲームが、`Runner` が返す「今の状態」を見て、自分で動く)。ゲーム固有の型は、エンジンに入れない(汎用の値の入れ物 = `Context` で渡す)。
- **ノードの種類を足す**:`src/Behavior/NodeTypes.cpp` の `MakeBuiltinTypes` に、登録を 1 つ書く(エディタの欄・メニューは、登録から自動で作られる)。条件の評価は `src/Behavior/Condition.cpp`。ゲームの動きは、ゲーム側。
- **グラフの JSON は、版つき**(`"version": 1`)。形式を変えたら、`GraphFile.cpp` の `FileVersion` を上げて、`docs/Behavior.md` に書く。壊れたファイル・欠けた項目でも、落ちない(エラー + 安全な既定)。
- **imnodes は、エンジンの中だけ(PRIVATE)**。公開ヘッダー(`GraphEditor.h` など)に、imnodes の型を出さない(pimpl)。コミットとハッシュを固定して取り込む(`CMakeLists.txt`)。ズームの機能は、imnodes に無い(既知の制限)。
- **ノードは、並びの番号ではなく、id で探す**(`Graph::FindNode`)。id は使い回さない(`nextNodeId` を JSON に保存)。
- **ツール `KRBehaviorEditor`**(`tools/BehaviorEditor/`)は、開いたゲームの `Assets/Enemies/<名前>.json` に保存する(`KR::SetAssetRoot`。ゲームは、呼ばない)。フォルダーを選ぶ窓は、`tools/Common/`(`KRToolCommon`)で、調整ツールどうしで共有。見本は、サンプル `KRBehavior`(`samples/Behavior/`)。
- 確認用の使い捨て exe は、`probe/BehaviorProbe/`(`-DKR_BUILD_PROBES=ON` のときだけ)。

## 文字コード・パス
- ソースは UTF-8(`/utf-8`)。**日本語を含むフォルダーに置いても動く**ように、CMake が埋め込んだ場所(`KR_*_SOURCE_DIR`)は、`char8_t` の列として `std::filesystem::path` にする(`Assets.cpp` の `Utf8Path`)。`path(const char*)` は、日本語 Windows で、CP932 として読まれて、例外になる
- 引数・設定ファイルなどで、UTF-8 の文字列と `path` を行き来するときも、`u8string()` / `char8_t` を使う(ツールの `EditorApp.cpp` の `ToUtf8` / `FromUtf8` が見本)
- 動作確認は、「プロセスが生きている」だけでなく、ウィンドウのタイトルが取れること(エラーのダイアログが出ていないこと)まで確かめる
