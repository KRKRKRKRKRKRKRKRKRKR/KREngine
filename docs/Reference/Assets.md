---
title: Assets(アセットの場所)
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# Assets

## 概要
ゲームのデータ(モデル・音・コース・設定など)の**フォルダーの場所**を返す関数。ゲームの `Assets/` フォルダーの下に、`Models/`・`Sounds/`・`Enemies/` などを置く。

**Debug と Release で、場所が違う**。その違いを、ここ 1 か所にまとめてある(別々に書くと、直し忘れる)。

| | 場所 |
|---|---|
| Debug | 編集中の元のフォルダー(ゲームの `Assets/<name>`。CMake の `KR_GAME_ASSETS_DIR`)。保存した変更が、コピーなしで、すぐ読める |
| Release | exe の横のフォルダー(`<exe のあるフォルダー>/<name>`)。ビルドの後に、元のフォルダーから、自動でコピーされる |

ゲームの `CMakeLists.txt` は、`kr_add_game(ゲーム名)` と書く。ゲームのアセットは、**省略すると、ゲームのフォルダーの Assets/**(別の場所にするなら、エンジンを取り込む前に `set(KR_GAME_ASSETS_DIR 場所)`)。`Assets/` の下のフォルダーは、`kr_add_game` が、exe の横へ、自動でコピーする(`Assets/` に、新しいフォルダーを足したときは、**次のビルドで自動で設定し直し、その次のビルドでコピー**される。書き方は `docs/Usage.md` の 2-2。0.13.0 までの `kr_copy_runtime_files(ゲーム名)` を使う書き方も、動く)。モデル・音・敵のグラフは、それぞれ `AnimatedModel::Load()`・`Audio::Load()`・`LoadGraph()` が、`Models/`・`Sounds/` の下から読むので、普通は、パスを自分で組み立てなくてよい。

ヘッダー:`KR/Core/Assets.h`

## サンプルコード
```cpp
// 敵のグラフのフォルダー(Assets/Enemies/)の下の Walker.json
const std::filesystem::path file = KR::GetAssetDirectory("Enemies") / "Walker.json";
KR::Behavior::GraphPtr graph = KR::Behavior::LoadGraph(file);

// 自分のデータ(Assets/Courses/ の下)
const std::filesystem::path courses = KR::GetAssetDirectory("Courses");
for (const auto& entry : std::filesystem::directory_iterator(courses))
{
    KR_LOG_INFO("コース: {}", entry.path().filename().string());
}
```

## 関数

### KR::GetAssetDirectory()　アセットのフォルダーの場所
```cpp
std::filesystem::path GetAssetDirectory(const std::filesystem::path& name);
```
ゲームのアセットの、`name` フォルダーの場所を返す。

| 引数 | 説明 |
|---|---|
| `name` | アセットの下のフォルダー名(例 `"Courses"`・`"Enemies"`) |

### KR::GetEngineShaderDirectory()　エンジンのシェーダーの場所
```cpp
std::filesystem::path GetEngineShaderDirectory();
```
エンジンのシェーダーのフォルダーの場所。シェーダーは、ゲームではなく、エンジンの物。Debug では、F10 で再読み込みできる。**普通は、ゲームは使わない**。

### KR::SetAssetRoot()　アセットの場所を、実行時に決める(ツール用)
```cpp
void SetAssetRoot(const std::filesystem::path& root);
```
アセットのフォルダー(`<root>/Models` など、`name` フォルダーの親)を、実行時に決める。別のゲームのデータを開いて、そこに保存する**ツール**(調整ツールなど)が使う。**ゲームは、呼ばない**。空の `path` を渡すと、解除する。

### KR::GetAssetRoot()　決めた場所を見る
```cpp
const std::filesystem::path& GetAssetRoot();
```
`SetAssetRoot` で決めた場所。決めていなければ、空。

## 列挙型・構造体
なし。

## アクセサー一覧
| 関数 | 説明 |
|---|---|
| `GetAssetRoot()` | `SetAssetRoot` で決めた場所 |
