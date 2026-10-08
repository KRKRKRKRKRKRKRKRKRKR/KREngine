# KREngine(KR エンジン)

進め方(ペアプログラミングのルール)は、共通。`C:\SchoolProgram\Game\Shared\CLAUDE.md`(`Game\CLAUDE.md` から自動で読まれる)。

DirectX 12 / C++23 / Windows の自作ゲームエンジン。ゲームからは、Git サブモジュール(`External/KREngine`)で使われる。

## このプロジェクト固有のこと
- 資料は `docs/`。目次は `docs/Structure.md`、使い方は `docs/Usage.md`
- ビルド(エンジン単体):`cmake --preset vs2026` → `cmake --build build --config Debug --target KRMinimal`
- 依存のルール:ゲームが使えるのは公開側 `include/KR/` だけ。`src/` と DX12 の部品は、ゲームから見せない。ゲームは GPU の実体に触らず、番号札(MeshHandle など)だけを持つ
- ゲーム固有のこと(自機・コース・ゲームの設定)は、エンジンに入れない。ゲームのアセットの場所は `KR_GAME_ASSETS_DIR`(ゲームの CMake が決める)
- エンジンを変えたら、`docs/` の該当ファイルも直す。ゲーム側が使うサブモジュールの指す版は、ゲーム側で進める
- コミットメッセージは日本語で、段階の番号を付ける(例:`切り出し E-5: …`)
