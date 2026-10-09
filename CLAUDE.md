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
- **判定の形(`Hitbox`)は、骨の中の座標で持つ**。保存は `<モデル名>.hitboxes.json`(nlohmann/json は、エンジンの中だけ = PRIVATE)。骨は**名前**で探す(無ければ番号)
- **調整は、ツール `KRAnimationEditor`(`tools/AnimationEditor/`)で行い、ゲームは、読み込むだけ**。ツールは、開いたゲームの `Assets` フォルダーに保存する(`KR::SetAssetRoot`)。**`SetAssetRoot` は、ツール用。ゲームは、呼ばない**
- 調整ツールの、画面の上の操作(クリックで選ぶ・ギズモで動かす・コピー・左右反転)は、`docs/AnimationEditor.md`。ギズモは、外部ライブラリ ImGuizmo(**ツールだけ**。エンジン本体・ゲームには入れない)。T = 移動 / S = 大きさ(Blender 準拠)
- 使い方の見本は `samples/Animated/`(KRAnimated)。骨つきのメッシュは、1 フレームに 32 個・骨は 128 本まで(`Renderer::MaxSkinnedDrawsPerFrame` / `MaxSkinJoints`)
- 手前に描く(`overlay`。深度テストなし)は、骨つきの形には使えない

## 文字コード・パス
- ソースは UTF-8(`/utf-8`)。**日本語を含むフォルダーに置いても動く**ように、CMake が埋め込んだ場所(`KR_*_SOURCE_DIR`)は、`char8_t` の列として `std::filesystem::path` にする(`Assets.cpp` の `Utf8Path`)。`path(const char*)` は、日本語 Windows で、CP932 として読まれて、例外になる
- 引数・設定ファイルなどで、UTF-8 の文字列と `path` を行き来するときも、`u8string()` / `char8_t` を使う(ツールの `EditorApp.cpp` の `ToUtf8` / `FromUtf8` が見本)
- 動作確認は、「プロセスが生きている」だけでなく、ウィンドウのタイトルが取れること(エラーのダイアログが出ていないこと)まで確かめる
