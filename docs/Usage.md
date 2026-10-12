# KR エンジンの使い方(最短の入門)

KR エンジン(DirectX 12 / C++23。Windows)で、**新しいゲームを作る手順**を、最短でまとめる。細かい仕組みは、この `docs/` の、ほかのファイル([Structure.md](Structure.md) が目次代わり)を見る。

**この文書の例は、`samples/Minimal/`(ウィンドウ + 回るキューブ + ImGui の FPS)が、そのまま、動く見本**。まず、それをビルドして、動かして、コードを読むのが、いちばん早い。

---

## 0. 必要な物
| 物 | 内容 |
|---|---|
| Windows 10 / 11 | DirectX 12 が動く PC |
| Visual Studio 2026(または、C++ のコンパイラ) | C++ のデスクトップ開発・Windows SDK |
| CMake 3.28 以上 | |
| インターネット | 初回の CMake が、DirectX-Headers・Dear ImGui・stb_image を、自動でダウンロードする |

## 1. サンプルを動かす(エンジン単体)
```
cd KREngine
cmake --preset vs2026
cmake --build build --config Debug --target KRMinimal
build/samples/Minimal/Debug/KRMinimal.exe
```
ウィンドウに、回る 6 色のキューブと、FPS が出れば、成功。

## 2. 新しいゲームを作る(サブモジュールで、取り込む)

### 2-1. ゲームのリポジトリの形
```
MyGame/
├─ External/KREngine/       ← エンジン(Git サブモジュール)
├─ Assets/                  ← ゲームのアセット(Models/・など。無くてもよい)
├─ src/ MyGame.cpp ...      ← ゲームのコード
└─ CMakeLists.txt
```
```
git init
git submodule add https://github.com/<アカウント>/KREngine.git External/KREngine
```
別の PC では、`git clone --recurse-submodules <ゲームの URL>`(すでに clone したなら `git submodule update --init`)。

### 2-2. ゲームの `CMakeLists.txt`(`samples/Minimal/CMakeLists.txt` を、見本に)
```cmake
cmake_minimum_required(VERSION 3.28)
project(MyGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ゲームのアセット(モデル・コース・設定など)の場所を、エンジンに教える(エンジンを取り込む前に決める)
set(KR_GAME_ASSETS_DIR ${CMAKE_SOURCE_DIR}/Assets)

# エンジンを取り込む。KR_ENGINE_PATH を指定すると、サブモジュールの代わりに、手元のエンジンのフォルダーを使える(エンジンを、同時に開発するとき)
if(KR_ENGINE_PATH)
    add_subdirectory(${KR_ENGINE_PATH} ${CMAKE_BINARY_DIR}/KREngine)
else()
    add_subdirectory(External/KREngine)
endif()

add_executable(MyGame src/MyGame.cpp)
target_link_libraries(MyGame PRIVATE KREngine)

# exe の横に、DLL・エンジンのシェーダー・ゲームのアセット(KR_GAME_ASSETS_DIR の下の全部のフォルダー)を、コピーする
kr_copy_runtime_files(MyGame)

# Debug: コンソールあり(ログが見える) / Release: コンソールなし。どちらも main() から起動させる(main は、エンジンが持っている)
set_target_properties(MyGame PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
target_link_options(MyGame PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
```

## 3. ゲームのコード(最小)
```cpp
#include <KR/KR.h>        // エンジンの、主なヘッダーを、まとめて取り込む
#include <imgui.h>

using namespace DirectX;

class MyGame : public KR::Application
{
public:
    MyGame() : Application(KR::ApplicationDesc{ .title = "MyGame" }) {}   // ウィンドウのタイトル(大きさも、指定できる)

protected:
    void OnInit() override      { /* 最初に 1 回:形・画像を読み込む */ }
    void OnUpdate(float dt) override { /* 毎フレーム:動かす。dt = 前のフレームからの秒数 */ }
    void OnRender() override    { /* 毎フレーム:描く(DrawMesh などで、描画リストに積む) */ }
    void OnImGui() override     { /* 毎フレーム:デバッグの窓(ImGui:: の関数) */ }
    void OnShutdown() override  { /* 最後に 1 回:番号札を返す */ }
};

// エンジンの main() が呼ぶ。作ったゲームを返す(これだけ書けば、main は、いらない)
std::unique_ptr<KR::Application> KR::CreateApplication() { return std::make_unique<MyGame>(); }
```

`OnInit` → ループ(`OnUpdate` → `OnRender` → `OnImGui`)→ `OnShutdown`、という順番で、呼ばれる。

## 4. よく使う、呼び出し

### 4-1. 形を作る・描く(`Renderer`。`GetRenderer()` で取る)
| やりたいこと | 呼び出し |
|---|---|
| キューブの、形のデータ | `KR::CreateCubeMeshData()` / `KR::CreateCubeMeshData(色)` |
| 市松模様の、平らな板 | `KR::CreateCheckerPlaneMeshData(タイルの数 X, タイルの数 Z, タイルの大きさ, 角の位置, 色 A, 色 B)` |
| OBJ ファイルから、形を読む | `KR::LoadObj("Folder/model.obj")`(ゲームのアセットの `Models/` からの、相対パス。失敗したら、エラーを出して、空の形を返す) |
| 形を、GPU に置く | `MeshHandle mesh = GetRenderer().CreateMesh(データ)`(**番号札**が、返る。失敗したら、`mesh.IsValid()` が false) |
| 形を、描く | `GetRenderer().DrawMesh(mesh, 行列)`(`OnRender` で。**その場では、描かない**。描画リストに積んで、フレームの最後に、まとめて描く)。画像を貼るなら、3 つ目の引数に `TextureHandle` |
| 形を、捨てる | `GetRenderer().DestroyMesh(mesh)`(番号札は、すぐ無効になる。GPU が、使い終わってから、実際に捨てる) |
| 画像(PNG など)を、読む | `TextureHandle texture = GetRenderer().CreateTexture(ファイルのパス)`。捨てるときは `DestroyTexture` |
| 線を引く(デバッグ用) | `GetRenderer().DrawLine(始点, 終点, 色)` / `DrawWireBox(OBB, 色)` |

**行列**は、`DirectXMath`:`XMMatrixScaling` → `XMMatrixRotationY` など → `XMMatrixTranslation` の順に、**掛ける**(S → R → T)。

### 4-2. カメラ・光・空・フォグ(毎フレーム、`OnUpdate` で、渡す)
```cpp
KR::Camera camera;
camera.SetPosition({ 0, 2, -5 });   // 位置(メートル)
camera.LookAt({ 0, 0, 0 });         // 見る先
camera.SetFieldOfView(60.0f);       // 視野(度)
camera.SetRollDegrees(0.0f);        // 傾き(度)
GetRenderer().SetCamera(camera);

GetRenderer().SetLighting(KR::Lighting{});  // 光(toLight・color・ambient)
GetRenderer().SetSky(KR::Sky{});            // 空(地平線・天頂・下の色)
GetRenderer().SetFog(KR::Fog{});            // フォグ(enabled・density)
```
渡さなければ、初期値で描く。座標系:**左手系**(+X が右・+Y が上・+Z が奥)。

### 4-3. 入力(`KR::Input`)
```cpp
KR::Input::IsKeyDown(KR::Key::W)      // 今、押している
KR::Input::IsKeyPressed(KR::Key::R)   // このフレームで、押した(最初の 1 フレームだけ true)
KR::Input::IsKeyReleased(KR::Key::R)  // このフレームで、離した
```
`Key`:`W A S D / Left Right Up Down / R Enter Space / F3`。足したいキーは、`KR/Core/Input.h` と `src/Core/Input.cpp` の対応表に、1 行ずつ。

### 4-4. 当たり判定(`KR/Physics/Collision.h`)
```cpp
KR::OBB a = KR::MakeOBB(中心, 半分の大きさ, 回転の行列);   // 向きを持てる箱
KR::OBB b = KR::MakeOBB(...);
bool hit = KR::Intersects(a, b);                           // 重なっているか(分離軸定理)
std::optional<float> t = KR::Raycast(ray, a);              // レイが当たるか(当たれば距離)。Ray は、Camera::ScreenPointToRay(x, y, 幅, 高さ) で作れる。球・線分は、[Collision.md](Collision.md)
```

### 4-5. ログ
`KR_LOG_TRACE / INFO / WARN / ERROR / FATAL("文字 {}", 値)`(`std::format` と同じ書き方)。Release では TRACE と INFO は、消える。

### 4-6. アセットの場所
| 呼び出し | 返す場所 |
|---|---|
| `KR::GetAssetDirectory("Courses")` | ゲームのアセットの `Courses/`(Debug: `KR_GAME_ASSETS_DIR` の元フォルダー。Release: exe の横) |
| `KR::GetEngineShaderDirectory()` | エンジンのシェーダー(Debug: エンジンの `shaders/` の元フォルダー。Release: exe の横の `Shaders/`) |

## 5. Debug と Release の違い
| | Debug | Release |
|---|---|---|
| コンソール | ある(ログが見える) | ない |
| アセット・シェーダー | **元のフォルダーを、直接読む**(保存すると、すぐ反映) | **exe の横に、ビルドのたびに、コピー**されたもの |
| シェーダーの再読み込み | **F10**(再起動なしで、反映) | 意味がない(コピーを読むため) |
| デバッグ層 | DirectX の、デバッグ層が、有効(遅いが、エラーを、詳しく出す) | 無効 |

そのほかの、キー:**F11** ボーダーレスの切り替え。

## 6. エンジンを、ゲームと一緒に開発する(サブモジュール)

**エンジンの版**:`メジャー.マイナー.パッチ`(例 0.2.1)。サブモジュールは、タグ(`git checkout v0.2.1`)で、指せる。進める前に、[CHANGELOG.md](../CHANGELOG.md) の「ゲーム側で必要な変更」を読む。決まりは [Versioning.md](Versioning.md)。

| 場面 | 手順 |
|---|---|
| エンジンを直して、ゲームで、すぐ試す(push しない) | エンジンを、別の場所に clone して(例:`C:/SchoolProgram/Game/KREngine`)、ゲームの configure に `-DKR_ENGINE_PATH=C:/SchoolProgram/Game/KREngine` を付ける(自分の PC だけの設定。`CMakeUserPresets.json` に書くと、楽。コミットしない) |
| エンジンの変更を、ゲームの、正式な版に、する | **① エンジンのリポジトリで、コミット・push** → **② ゲームのリポジトリで、`External/KREngine` が、新しいコミットを指している変更を、コミット**(順番を守る。逆にすると、別の PC で、そのコミットが、見つからない) |
| サブモジュールの中を、直す | サブモジュールの中は、特定のコミットに固定(`detached HEAD`)。直す前に、`git checkout main` |
| 別の PC で、更新を、取り込む | ゲームで `git pull` したあと、`git submodule update` |

## 日本語などを含むフォルダーに置いたとき(Debug)
Debug ビルドは、エンジンの `shaders/` と、ゲームのアセット(`KR_GAME_ASSETS_DIR`)を、元のフォルダーから直接読む(場所は、CMake が文字列として埋め込む)。埋め込んだ文字列は UTF-8 なので、`GetAssetDirectory` / `GetEngineShaderDirectory`(`src/Core/Assets.cpp`)は、UTF-8 として `std::filesystem::path` にしている。そのため、フォルダー名に日本語が入っていても動く(以前は、日本語 Windows で、起動直後に `std::system_error` で落ちた)。

## 7. 動くモデル(ボーンアニメーション)を使う

Blender などで作った、骨つきのモデル(glTF = `.glb`)を、動かして、描いて、当たり判定に使う。**`KR::AnimatedModel` だけ**で、できる。見本は、サンプル `samples/Animated/`(KRAnimated。約 60 行)。判定の形の調整は、ツール [KRAnimationEditor](AnimationEditor.md) で行う。

### 7-1. モデルの用意(Blender)
- 書き出しの形式は **glTF Binary (.glb)**(または、FBX。下)。骨(アーマチュア)とアニメーションを、含める設定にする。
- 置き場所:ゲームのアセットの `Models/` の下(例 `Assets/Models/Boss/Boss.glb`)。コードでは、`Models/` からの相対パス(`"Boss/Boss.glb"`)で書く。
- 骨の名前は、英数字がおすすめ(日本語でも動くが、ImGui の既定のフォントでは、表示が出ないことがある)。1 つの形(メッシュ)に付けられる骨は、最大 128 本。1 頂点が引っぱられる骨は、最大 4 本。
- **FBX(`.fbx`)も、同じ呼び出しで読める**(`AnimatedModel::Load("Boss/Boss.fbx")`。`.glb` との違いは、拡張子だけ)。メッシュ・骨・スキン・動きを読む。マテリアル・画像は、読まない(ゲーム側で、別に指定する。glTF と同じ)。同名の `.glb` と `.fbx` は、`.hitboxes.json` を共有する([AnimationEditor.md](AnimationEditor.md))。
  - **Blender の書き出し(FBX)**:Export → FBX。おすすめの設定:
    - **Selected Objects**(選んだ物だけ)にして、モデルのメッシュとアーマチュアだけを、書き出す(判定用の箱・カメラ・ライトを、含めない)。
    - **Scale: 1.0**・**Apply Scalings・Forward・Up は、Blender の既定のまま**(Forward = -Z・Up = Y)。この既定で書き出した `BossObstacle.fbx`(単位 cm・アーマチュアに 100 倍の拡大が付く形)が、正しい大きさ・向きで読めることを確認した。軸と単位の違いは、読み込み時に、ufbx が、メートル・Y が上に直す。(設定の名前は、Blender の版で、少し違うことがある)
    - **Add Leaf Bones: オフ**(先端に、余分な骨が、増えない)。
    - **Armature → Only Deform Bones: オン**・**Bake Animation: オン**(動きを、含める)。
    - **アーマチュアとメッシュの置き場所を、原点にそろえる**(Object → Apply → All Transforms)。そろっていないと、骨と判定の形が、メッシュから離れた所に出る(読み込み時に、警告が出る)。
  - **Mixamo など、外部の FBX**:メッシュ・骨・動きは、読める。**ライセンス(再配布の条件)は、自分で確認する**(リポジトリには、確認できるまで、入れない)。骨の名前は、`mixamorig:` なども、そのまま使う。
  - 1 つの頂点が引っぱられる骨は、最大 4 本(5 本以上の頂点は、重みの大きい 4 本に絞る。ログに、数が出る)。モーフ(ブレンドシェイプ)は、読まない(警告を出して、読み飛ばす)。

### 7-2. コード(最小)
```cpp
class MyGame : public KR::Application
{
protected:
    void OnInit() override
    {
        m_Boss.Load(GetRenderer(), "Boss/Boss.glb");   // 形・骨・動き・判定の形を読む。最初の動きを、ループで再生しはじめる
        m_Boss.Play("Walk");                            // 動きを、名前か番号で選ぶ(ループしないなら、第 2 引数 false)
    }
    void OnUpdate(float deltaTime) override
    {
        m_Boss.Update(deltaTime);                       // 時刻を進めて、姿勢を求める
    }
    void OnRender() override
    {
        const XMMATRIX world = XMMatrixTranslation(0.0f, 0.0f, 20.0f); // モデル全体の置き方(位置・向き・拡大)
        m_Boss.Draw(GetRenderer(), world);              // 今の姿勢で、描く(骨つき・骨なしは、自動で選ぶ)
    }
    void OnShutdown() override
    {
        m_Boss.Unload(GetRenderer());
    }
private:
    KR::AnimatedModel m_Boss;
};
```

| 呼び出し | 内容 |
|---|---|
| `Load(renderer, 相対パス)` | glTF を読む。失敗したら、エラーを出して `false` |
| `Play(名前 or 番号, loop)`・`Stop()`・`Pause()`・`SetSpeed()`・`SetTime()` | 再生の操作。`IsFinished()`:ループしない動きが、最後まで来た |
| `Update(deltaTime)` | 毎フレーム 1 回 |
| `Draw(renderer, world, texture)` | 描く |
| `GetWorldHitboxes(world)` | 今の姿勢での、判定の形(箱・球)。`Draw` に渡した `world` と同じ物を渡す |
| `GetSkeleton()`・`GetPose()`・`GetBounds()` | 骨・今の姿勢・メッシュを囲む箱(デバッグの表示・カメラの位置などに) |

### 7-3. 当たり判定(骨に追従する箱・球)
```cpp
const std::vector<KR::WorldHitbox> shapes = m_Boss.GetWorldHitboxes(world);
for (const KR::WorldHitbox& shape : shapes)
{
    if (KR::Intersects(shape, playerShape)) { /* 当たった */ }   // 箱・球の、どの組み合わせでも、同じ関数
}
```
- 判定の形は、**骨の中の座標**で持つ(骨が動いても、数字は変わらない。骨の今の位置・向きに、自動で追従する)。
- 形は、`Boss.hitboxes.json`(モデルの隣)に、保存される。無ければ、骨ごとの、とりあえずの形(子がある骨 = 箱・先端 = 球)になる。
- **調整のしかた**:ツール **KRAnimationEditor**([AnimationEditor.md](AnimationEditor.md))で、ゲームのアセットのフォルダーを開いて、骨を選び、判定の形を動かして(中心・大きさ)、「Save hitboxes」で保存する。ツールは、開いたフォルダーに保存する(Release でも、元のフォルダーに残る)。ゲームでは、読み込むだけ。
- 詳しくは [Animation.md](Animation.md)・[Collision.md](Collision.md)。

### 7-4. 注意
- ゲームの CMake は、そのままでよい(エンジンが、cgltf と nlohmann/json を取り込む)。ゲームが、同じ nlohmann/json(v3.12.0)を取り込んでいても、競合しない。
- 骨つきの物は、1 フレームに 32 個まで描ける(`Renderer::MaxSkinnedDrawsPerFrame`)。骨なしの `DrawMesh` に、骨つきのメッシュを渡すと、エラーになる(`AnimatedModel::Draw` を使えば、自動で選ばれる)。

## 8. 敵の行動(ノードで作った行動)を使う
**ツールでノードをつないで作った、敵の行動(状態機械)を、ゲームで動かす**。コードを書き換えずに、ツールで直して保存すれば、行動が変わる。しくみ・経緯・決めたことは [Behavior.md](Behavior.md)・ツールは [BehaviorEditor.md](BehaviorEditor.md)。動く見本は、サンプル `KRBehavior`(`samples/Behavior/`)。

- **エンジンは、「今、どの状態か」だけを返す**。移動・当たり判定・攻撃の時間の数え方は、**ゲームの仕事**(エンジンは、ゲーム固有の型を知らない)。ゲームは、状況を、名前つきの値(`Context`)で渡す。

### 8-1. グラフを作る(ツール)
1. ツール `KRBehaviorEditor` を起動する(`cmake --build build --config Debug` で、エンジン単体のビルドと一緒にビルドされる)。引数 `--assets <ゲームの Assets フォルダー>` で、そのフォルダーを開く(「参照...」でも選べる)。
2. 何もない所で右クリック → 状態(青)と条件(緑)を足す。**状態 → 条件 → 状態**の順に、ピンからピンへドラッグして、つなぐ(条件は、状態と状態の間)。「ここから始める」で、最初の状態を決める。値(速さ・半径・秒など)を、ノードの中で直す。
3. 左の名前の欄に名前(例 `Walker`)を書いて「保存」→ **`<Assets>/Enemies/Walker.json`** ができる。1 つの敵の種類 = 1 つのグラフ = 1 ファイル。
- 上に出る「警告」は、つなぎ忘れの知らせ(動きはする)。

### 8-2. ゲームのコード(契約に対応した、実際の呼び出し)
```cpp
#include <KR/KR.h>

// ① 起動時:グラフを読む(敵の種類ごとに 1 回。複数の敵で共有する)
//    読めなければ、エラーを出して、待機だけのグラフ(落ちない)
KR::Behavior::GraphPtr graph = KR::Behavior::LoadGraph(KR::GetAssetDirectory("Enemies") / "Walker.json");

// ② 敵ごとに、Runner を 1 つ(作った直後は、開始の状態に入っている)
KR::Behavior::Runner runner(graph);

// ③ 毎フレーム:状況を入れて、Update して、今の状態を見て、動く
KR::Behavior::Context context;
context.SetFloat("distanceToPlayerX", std::abs(playerX - enemyX));   // 横の距離
context.SetFloat("distanceToPlayerY", std::abs(playerY - enemyY));   // 縦も含める条件のときだけ使われる
context.SetBool("attackFinished", attackFinished);                   // 攻撃の溜め・出る・硬直を、数え終わったら true
runner.Update(deltaTime, context);

const KR::Behavior::StateView state = runner.GetCurrentState();     // Update・SetGraph のたびに、取り直す
if (state.GetType() == "Patrol")            { enemyX += direction * state.GetFloat("speed") * deltaTime; /* 壁で反転 */ }
else if (state.GetType() == "MoveToPlayer") { /* プレイヤーへ、state.GetFloat("speed") で */ }
else if (state.GetType() == "Attack")
{
    if (state.EnteredThisFrame()) { attackTime = 0.0f; }                      // 入った最初のフレームだけ true
    attackTime += deltaTime;
    // 溜め → 出ている → 硬直(秒は、state.GetFloat("windupSeconds") など)。箱は、state.GetFloat("boxWidth") など
    attackFinished = attackTime >= state.GetFloat("windupSeconds") + state.GetFloat("activeSeconds") + state.GetFloat("recoverySeconds");
}
// "Wait"(待機)は、何もしない

// ④ デバッグ(ImGui の窓の中で。今の状態のノードが光る)
KR::Behavior::DrawGraphDebug(runner);                 // Runner が持つグラフを描く(いちばん簡単)
// KR::Behavior::DrawGraphDebug(*graph, runner);      // グラフを指定するなら(Graph を渡す。GraphPtr なら *graph)
```
- **`StateView`**:`GetType()`(状態の種類)・`GetNodeId()`・`GetTimeInState()`(入ってからの秒)・`EnteredThisFrame()`・値(`GetFloat` / `GetInt` / `GetBool` / `GetString`。無い・型が違うときは、既定値を渡せる)。
- **1 回の `Update` で移る状態は、1 回まで**(状態が、1 フレームで、通り過ぎない = `EnteredThisFrame()` を、見逃さない)。
- **状況のキーが無い条件**は、偽になり、**警告が 1 回**ログに出る(キーの名前を、グラフの条件ノードの設定と、そろえる)。

### 8-3. 保存したら、動いているゲームに、すぐ反映する(`GraphAsset`)
```cpp
KR::Behavior::GraphAsset asset;
asset.Load(KR::GetAssetDirectory("Enemies") / "Walker.json");   // ① の代わり
KR::Behavior::Runner runner(asset.GetGraph());
uint32_t seen = asset.GetVersion();
// 毎フレーム:
asset.ReloadIfChanged(deltaTime);                              // 約 0.5 秒おきに、更新時刻を見る
if (asset.GetVersion() != seen) { runner.SetGraph(asset.GetGraph()); seen = asset.GetVersion(); }
```
- `Runner::SetGraph` は、**今の状態の id が、新しいグラフにも残っていれば、その状態と時間を引き継ぐ**。無ければ、開始の状態に戻る。敵が複数いるときは、敵ごとに `seen` を持つ。
- ツールで保存 → 約 0.5 秒で、動いている敵に反映される(Debug は、元の `Assets` を読むので、コピーが要らない)。**壊れたファイルは、前のグラフのまま**。

### 8-4. ノードの種類と、ゲームがすること
| 状態(`GetType()`) | 値(既定) | ゲームがすること |
|---|---|---|
| `Wait`(待機) | なし | 動かない |
| `Patrol`(巡回) | `speed`(1.5) | 左右に歩いて、壁で反転する(壁の判定は、ゲーム) |
| `MoveToPlayer`(プレイヤーへ移動) | `speed`(2.5) | プレイヤーへ近づく |
| `Attack`(攻撃) | `boxWidth`(1.0)・`boxHeight`(0.5)・`damage`(1)・`windupSeconds`(0.4)・`activeSeconds`(0.2)・`recoverySeconds`(0.4) | 溜め → 出ている(箱で、当たり判定)→ 硬直。**エンジンは、秒を数えない**。ゲームが数え、終わったら `attackFinished` を true にする |

| 条件 | 値(既定) | 使う Context のキー |
|---|---|---|
| `PlayerWithin`(プレイヤーが半径以内) | `key`("distanceToPlayerX")・`radius`(3.0)・`includeVertical`(false)・`keyY`("distanceToPlayerY") | `key`(縦も含めるなら、`keyY` も) |
| `PlayerOutside`(半径の外) | 同上(`radius` 6.0) | 同上 |
| `TimeElapsed`(秒たった) | `seconds`(1.0) | なし(状態に入ってからの時間 = エンジンが数える) |
| `AttackFinished`(攻撃が終わった) | `key`("attackFinished") | `key` |
- **距離は、横(X)だけが基本**。`includeVertical` が true のときは、縦(`keyY`)も含めて、√(x² + y²) で比べる。**ちょうど半径は、内側**。
- 1 つの状態から、条件を複数出せる(**分岐**)。調べる順番(priority)は、ツールの「分岐の順番」で決める。最初に成り立ったものへ移る。
- 同じ値の名前・ゲームの「状態の種類」の文字列は、`Behavior.md` の表と、ツールの登録(`NodeTypes.cpp`)が、元。**新しい種類を足すとき**は、ツール側は登録を 1 つ書くだけ(ノードの欄は、自動で作られる)。ゲーム側は、その `GetType()` を扱う分岐を足す。

### 8-5. 注意
- **Release では、`Enemies` フォルダーが、exe の横に、コピーされる**(ビルド後)。**`Enemies` を、初めて作ったあとは、CMake を、もう一度 configure する**(コピーする一覧は、configure のときに決まるため)。
- ゲームの CMake は、そのままでよい(imnodes・nlohmann/json は、エンジンの中だけ)。
- ファイルの形式(JSON)には、版(`"version": 1`)がある。**版が違うファイルは、読まない**(エラー + 待機だけのグラフ)。
- 壊れたファイル・欠けた項目でも、落ちない(エラー + 安全な既定)。
