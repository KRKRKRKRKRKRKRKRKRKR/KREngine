---
title: はじめてのエンジン:最短の流れと、関数の早見表
project: KREngine
kind: 使い方
status: 最新
updated: 2026-10-15
replaces:
---

# はじめてのエンジン:最短の流れと、関数の早見表

エンジンを**初めて使う人**が、最初に開く 1 枚。ゲームのコードで、何を書いて、どの関数を呼べばよいかを、**やりたいことから**探せる。詳しい説明は、各節の最後のリンク先(`Usage.md` など)。この文書は、公開ヘッダー(`include/KR/`)を読んで、書いた(版 0.13.0 時点)。

**エンジンのバージョン**:`KR::GetVersionString()` で分かる(例 `"0.13.0"`)。起動時のログの最初の行にも出る。決まりは [Versioning.md](Versioning.md)・変更の記録は [../CHANGELOG.md](../CHANGELOG.md)。

## 0. このエンジンでできること
| やりたいこと | 使う物 | 詳しくは |
|---|---|---|
| ウィンドウを出して、毎フレーム動かす | `KR::Application` | [Lifecycle.md](Lifecycle.md) |
| 3D の形(キューブ・板・OBJ)を描く | `Renderer`・`MeshData` | [Rendering.md](Rendering.md)・[Mesh.md](Mesh.md) |
| 画像(PNG など)を貼る | `Renderer::CreateTexture` | [Textures.md](Textures.md) |
| カメラ・光・空・霧 | `Camera`・`Lighting`・`Sky`・`Fog` | [Lighting.md](Lighting.md) |
| キーボード・マウス | `KR::Input` | [Input.md](Input.md) |
| 当たり判定(箱・球・レイ) | `OBB`・`Sphere`・`Intersects`・`Raycast` | [Collision.md](Collision.md) |
| 動くモデル(骨・動き。glTF / FBX) | `AnimatedModel` | [Animation.md](Animation.md) |
| 敵の行動(ノードで作る) | `KR::Behavior`(`Runner` など) | [Behavior.md](Behavior.md) |
| 音(効果音・BGM) | `Audio`(`GetAudio()`) | [Audio.md](Audio.md) |
| デバッグの窓 | ImGui(`OnImGui`) | [ImGui.md](ImGui.md) |

## 1. 最短で動かす
ゲームのコードで書くのは、**3 つだけ**。

1. `KR::Application` を継承したクラスを作る。
2. `OnInit`(最初に 1 回)・`OnUpdate`(毎フレーム。動かす)・`OnRender`(毎フレーム。描く)・`OnImGui`(デバッグの窓)・`OnShutdown`(最後に 1 回)を、必要な分だけ書く。
3. `KR::CreateApplication()` を書いて、そのクラスを返す(`main` は、エンジンが持っている)。

```cpp
#include <KR/KR.h>        // エンジンの、主なヘッダーを、まとめて取り込む
#include <imgui.h>        // OnImGui で ImGui:: を使うなら
#include <memory>

using namespace DirectX;

class MyGame : public KR::Application
{
public:
    MyGame() : Application(KR::ApplicationDesc{ .title = "MyGame" }) {}   // ウィンドウの題名(大きさも指定できる)

protected:
    void OnInit() override
    {
        m_Cube = GetRenderer().CreateMesh(KR::CreateCubeMeshData());     // 形を作って、番号札をもらう
    }
    void OnUpdate(float dt) override
    {
        m_Angle += dt;                                                    // dt = 前のフレームからの秒数
        KR::Camera camera;
        camera.SetPosition({ 0, 2, -5 });
        camera.LookAt({ 0, 0, 0 });
        GetRenderer().SetCamera(camera);                                  // カメラは、毎フレーム渡す
    }
    void OnRender() override
    {
        GetRenderer().DrawMesh(m_Cube, XMMatrixRotationY(m_Angle));      // 「この形を、この行列で描いて」と、積む
    }
    void OnShutdown() override { GetRenderer().DestroyMesh(m_Cube); }   // 番号札を返す

private:
    KR::MeshHandle m_Cube;
    float m_Angle = 0.0f;
};

std::unique_ptr<KR::Application> KR::CreateApplication() { return std::make_unique<MyGame>(); }
```
**ゲームの CMake・サブモジュール・ビルドの手順**は、[Usage.md](Usage.md) の 2。協力者として、すでにあるゲーム(すれすれ飛行)を動かすだけなら、そのゲームの `docs/GettingStarted.md`。**一番簡単な見本**:`samples/Minimal/MinimalApp.cpp`(キューブを回して、入力をログに出す)。

## 2. 1 フレームの流れ
```
OnInit  →  ループ{ OnUpdate(dt) → OnRender() → OnImGui() }  →  OnShutdown
```
| 関数 | いつ | 書くこと |
|---|---|---|
| `OnInit()` | 起動時に 1 回 | 形・画像・音・モデルを読む |
| `OnUpdate(float deltaTime)` | 毎フレーム | 入力を読む・動かす・カメラ・光を渡す |
| `OnRender()` | 毎フレーム | `DrawMesh` などで、**描く依頼を積む** |
| `OnImGui()` | 毎フレーム(`OnRender` の後) | `ImGui::Begin(...)` ~ `End()`。ゲームの絵の手前に乗る |
| `OnShutdown()` | 終了時に 1 回 | 番号札・モデルを返す |

**動きに `deltaTime` を掛ける**(1 秒あたりの量にする)と、どのモニターでも、同じ速さになる。

## 3. 最初に知っておく、5 つの考え方
1. **番号札**:形(`MeshHandle`)・画像(`TextureHandle`)・音(`SoundHandle` / `VoiceHandle`)は、GPU や OS の実体ではなく、**番号札**で持つ。`札.IsValid()` が false なら、失敗(エラーは、ログに出る)。
2. **描くのは、積むだけ**:`DrawMesh` はその場では描かない。描画リストに積んで、フレームの最後に、まとめて描く。**OnRender で呼ぶ**。
3. **座標系**:**左手系**(+X が右・+Y が上・+Z が奥)。長さの単位は**メートル**。行列は DirectXMath で、`拡大 × 回転 × 移動` の順に掛ける。
4. **アセットの場所**:ゲームの `Assets/` の下。コードでは `KR::GetAssetDirectory("Models")` のように取る(Debug は元のフォルダー・Release は exe の横)。モデルは `Assets/Models/`・音は `Assets/Sounds/`・敵のグラフは `Assets/Enemies/`。
5. **Debug と Release**:Debug は、ログが全部見える・F1(ツールを開く)・F10(シェーダーの再読み込み)が効く。Release は、`KR_LOG_TRACE` / `KR_LOG_INFO` が消える。

## 4. 関数の早見表
`#include <KR/KR.h>` で、全部使える。名前空間は `KR`(敵の行動は `KR::Behavior`)。

### 4-1. ゲームの本体(`KR::Application`)
| 呼び出し | 何をする |
|---|---|
| `KR::ApplicationDesc{ .title, .width, .height }` | ウィンドウの題名・大きさ(既定 1280 × 720) |
| `GetRenderer()` | 描画の窓口(`Renderer&`)。派生クラスの中で使う |
| `GetAudio()` | 音の窓口(`Audio&`) |
| `KR::GetVersionString()` / `KR::GetVersion()` | エンジンの版(`"0.13.0"` / `{major, minor, patch}`) |
| `KR::GetCommandLineArguments()` | 起動時の引数(先頭 = exe のパス。日本語のパスも読める) |

### 4-2. 描く(`Renderer`。`GetRenderer()` で使う)
| 呼び出し | 何をする |
|---|---|
| `KR::CreateCubeMeshData()` / `(色)` | キューブの形のデータ(1 辺 1 m・中心が原点。色を渡すと、全部の面が同じ色) |
| `KR::CreateCheckerPlaneMeshData(tilesX, tilesZ, tileSize, origin, colorA, colorB)` | 市松模様の平らな板 |
| `KR::LoadObj("Folder/model.obj")` | OBJ ファイルから、形のデータ(`Assets/Models/` からの相対パス。失敗したら空) |
| `MeshHandle CreateMesh(MeshData)` | 形を GPU に置く(番号札が返る) |
| `DestroyMesh(MeshHandle)` | 形を捨てる(札は、すぐ無効になる) |
| `TextureHandle CreateTexture(path)` / `DestroyTexture` | 画像(PNG など)を読む・捨てる。**起動時に読む**(走りながら作ると止まる) |
| `DrawMesh(mesh, world, texture = {}, overlay = false)` | 形を描く依頼を積む。`world` = 置き方の行列 |
| `DrawSkinnedMesh(mesh, world, palette, texture)` | 骨つきの形を描く(普通は、`AnimatedModel::Draw` が呼ぶ) |
| `DrawLine(start, end, color, overlay = false)` | デバッグの線 |
| `DrawWireBox(OBB, color)` / `DrawWireSphere(Sphere, color)` | 当たり判定の箱・球を、線で見る |
| `SetClearColor(r, g, b)` / `SetVSync(bool)` | 背景色・垂直同期 |

`overlay = true` は、深度テストなしで、ほかの物の**手前**に描く(骨つきの形には使えない)。骨つきの物は、1 フレームに 32 個・骨は 512 本まで。

### 4-3. カメラ・光・空・霧(毎フレーム、`OnUpdate` で渡す)
| 呼び出し | 何をする |
|---|---|
| `KR::Camera`:`SetPosition({x,y,z})`・`LookAt({x,y,z})`・`SetFieldOfView(度)`・`SetRollDegrees(度)`・`SetClipPlanes(近, 遠)` | どこから・どこを・どれくらいの範囲で見るか |
| `Camera::ScreenPointToRay(x, y, 幅, 高さ)` | 画面の点(マウスの位置)から、世界へ伸びる `Ray` |
| `Renderer::SetCamera(camera)` | このフレームのカメラ(渡さなければ、初期値) |
| `KR::Lighting`(`toLight`・`color`・`ambient`)→ `SetLighting` | 太陽の光と環境光 |
| `KR::Sky`(`horizonColor`・`zenithColor`・`nadirColor`・`horizonWidth`)→ `SetSky` | 空のグラデーション |
| `KR::Fog`(`enabled`・`density`)→ `SetFog` | 遠くが霞む霧(色は、空の地平線に合う) |

### 4-4. 入力(`KR::Input`。`OnUpdate` で読む)
| 呼び出し | 何をする |
|---|---|
| `Input::IsKeyDown(Key::W)` | 今、押している |
| `Input::IsKeyPressed(Key::Space)` | このフレームで押した(最初の 1 フレームだけ true) |
| `Input::IsKeyReleased(Key::Space)` | このフレームで離した |
| `Input::GetMousePosition()` | マウスの位置(`.x`・`.y`。左上が (0, 0)・画素) |
| `Input::GetMouseWheel()` | このフレームのホイール(奥へ回すと +) |
| `Input::GetKeyName(Key::Space)` | キーの名前の文字("Space") |

キー(`KR::Key`):`A`〜`Z`・`Num0`〜`Num9`・`F1`〜`F12`・`Left Right Up Down`・`Enter Space Escape Tab Backspace`・`Shift Ctrl Alt`・`Insert Delete Home End PageUp PageDown`・`Numpad0`〜`Numpad9` ほか・**マウスのボタンも同じ列挙**(`MouseLeft MouseRight MouseMiddle`)。ImGui の窓を使っている間は、「押した」とホイールが、ゲームに届かない。`F10`・`F11` は、エンジンも使う。

### 4-5. 当たり判定(`KR/Physics/Collision.h`)
| 呼び出し | 何をする |
|---|---|
| `KR::MakeOBB(center, halfSize, rotation)` | 向きを持てる箱(OBB)を作る。回転しない箱も、同じ形で持てる |
| `KR::Sphere{ center, radius }` | 球 |
| `KR::Intersects(a, b)` | 重なっているか。**OBB どうし・球どうし・球と OBB**(どちらの順でも) |
| `KR::Raycast(ray, sphere or box)` | レイが当たるか。当たれば、始点からの距離 `t`(`std::optional<float>`) |
| `KR::RayToSegmentDistance(ray, a, b)` | レイと線分の最短距離(細い物をマウスで選ぶ) |

### 4-6. ログ・アセットの場所
| 呼び出し | 何をする |
|---|---|
| `KR_LOG_INFO("文字 {}", 値)` | `std::format` と同じ書き方。`KR_LOG_TRACE / INFO / WARN / ERROR / FATAL`(`FATAL` は、出力のあと、止まる) |
| `KR::GetAssetDirectory("Models")` | ゲームのアセットの `Models/` の場所(`std::filesystem::path`) |
| `KR::GetEngineShaderDirectory()` | エンジンのシェーダーの場所(普通は、使わない) |

**KR::SetAssetRoot は、ツール用**。ゲームは、呼ばない。

### 4-7. 動くモデル(`KR::AnimatedModel`。glTF / FBX。ゲームは、これだけ使う)
```cpp
KR::AnimatedModel boss;
boss.Load(GetRenderer(), "Boss/Boss.glb");   // OnInit:Assets/Models/ からの相対パス(.glb / .gltf / .fbx)。失敗したら false
boss.Play("Walk");                            // 動きを選ぶ(名前か番号)。ループしないなら 第 2 引数 false
boss.Update(dt);                              // OnUpdate:時刻を進めて、姿勢を求める
boss.Draw(GetRenderer(), world);              // OnRender:骨つきのメッシュを描く
auto hit = boss.GetWorldHitboxes(world);      // 今の姿勢の、判定の形(箱・球)。KR::Intersects(WorldHitbox, WorldHitbox) で調べる
boss.Unload(GetRenderer());                   // OnShutdown
```
| 呼び出し | 何をする |
|---|---|
| `Play(名前 or 番号, loop = true)` / `Stop()` / `Pause(bool)` | 再生・停止・一時停止 |
| `SetSpeed(倍率)` / `SetLooping(bool)` / `SetTime(秒)` | 速さ(負は逆再生)・ループ・時刻 |
| `IsFinished()` | ループしない動きが、最後まで終わったか |
| `GetClipCount()` / `GetClipName(i)` / `GetDuration()` | 動きの数・名前・長さ |
| `GetBounds()` | モデルを囲む箱(カメラを置くときなど) |
| `GetHitboxes()` / `SaveHitboxes()` / `ReloadHitboxes()` | 判定の形の編集・保存・読み直し(普通は、ツールで調整する) |

**判定の形は、ツール `KRAnimationEditor` で調整する**(ゲームは、読むだけ)。モデルの用意(Blender)・注意は [Usage.md](Usage.md) の 7。

### 4-8. 敵の行動(`KR::Behavior`。ツール `KRBehaviorEditor` で作ったグラフを、動かす)
```cpp
// ① 起動時:グラフを読む(敵の種類ごとに 1 回。複数の敵で共有する)
KR::Behavior::GraphPtr graph = KR::Behavior::LoadGraph(KR::GetAssetDirectory("Enemies") / "Walker.json");
// ② 敵ごとに Runner を 1 つ
KR::Behavior::Runner runner(graph);
// ③ 毎フレーム:状況を入れて、Update して、今の状態を見て、ゲームが動く
KR::Behavior::Context context;
context.SetFloat("distanceToPlayerX", std::abs(playerX - enemyX));
context.SetBool("attackFinished", attackFinished);
runner.Update(deltaTime, context);
const KR::Behavior::StateView state = runner.GetCurrentState();
if (state.GetType() == "Patrol") { /* state.GetFloat("speed") で動く */ }
// ④ デバッグ(ImGui の窓の中で):今の状態のノードが光る
KR::Behavior::DrawGraphDebug(runner);
```
| 呼び出し | 何をする |
|---|---|
| `LoadGraph(path)` | JSON のグラフを読む。壊れていても落ちない(待機だけのグラフ) |
| `Runner(graph)` / `Update(dt, context)` / `GetCurrentState()` / `Reset()` / `SetGraph(graph)` | 敵 1 体の「今の状態」を決める |
| `Context::SetFloat / SetInt / SetBool(キー, 値)` | 今の状況をゲームが入れる(キーの名前は、グラフの条件と揃える) |
| `StateView::GetType()` / `GetFloat(名前)` / `GetTimeInState()` / `EnteredThisFrame()` | 今の状態の種類・値・入ってからの秒・入った最初のフレームか |
| `GraphAsset`:`Load` / `ReloadIfChanged(dt)` / `GetVersion()` | ファイルが書き換わったら、動いているゲームに、自動で反映 |
| `ObserveRunner(runner, asset)` | 見てほしい敵を、毎フレーム、ツールへ知らせる(Debug だけ。F1 のツールのグラフで、今の状態が光る) |
| `DrawGraphDebug(runner)` | ImGui の窓の中に、グラフを描く(読むだけ) |

**エンジンは、物理・移動・当たり判定を持たない**。ゲームが、`Runner` が返す「今の状態」を見て、自分で動く。状態の種類(`Wait`・`Patrol`・`MoveToPlayer`・`Attack`)・条件(`PlayerWithin` など・かつ・または・でない)は、[Usage.md](Usage.md) の 8-4。**Debug で F1 を押すと、ツールが開く**(ゲームを止める / 動かしたまま直す、を切り替えられる)。

### 4-9. 音(`GetAudio()`。WAV・OGG・MP3)
```cpp
m_Jump = GetAudio().Load("jump.wav");               // OnInit:ゲームの Assets/Sounds/ からの場所
GetAudio().Play(m_Jump);                            // 効果音(重ねて鳴らせる)
m_Bgm = GetAudio().Play(m_Music, true, 0.5f);       // BGM(ループ・音量 0.5)
GetAudio().Stop(m_Bgm);
```
| 呼び出し | 何をする |
|---|---|
| `Load(path)` / `Unload(sound)` | 音を読む・捨てる(札は `SoundHandle`) |
| `Play(sound, loop = false, volume = 1.0f)` | 鳴らす。`VoiceHandle` が返る |
| `Stop(voice)` | 止める(鳴り終わった札でも、何もしないだけ) |
| `SetMasterVolume(0〜1)` / `GetMasterVolume()` | 全体の音量 |

音の機器が無くても、落ちない(警告だけ)。BGM は、継ぎ目に隙間が出にくい **OGG** を勧める。

### 4-10. デバッグの窓(ImGui)
`OnImGui()` の中で、`ImGui::Begin("名前")` ~ `ImGui::End()`。`#include <imgui.h>`。例:`ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);`。調整した値を JSON に保存する例は、ゲーム「すれすれ飛行」の `docs/Settings.md` が参考になる。

## 5. やりたいことから探す(逆引き)
| やりたいこと | 書くこと |
|---|---|
| キューブを置く | `CreateMesh(CreateCubeMeshData())` → `OnRender` で `DrawMesh(mesh, 行列)` |
| 地面を作る | `CreateMesh(CreateCheckerPlaneMeshData(...))` |
| 3D モデル(動かない)を出す | Blender で OBJ を書き出す → `LoadObj("X/X.obj")` → `CreateMesh` |
| 3D モデル(動く)を出す | `AnimatedModel` の `Load` / `Play` / `Update` / `Draw` |
| キーで動かす | `OnUpdate` で `Input::IsKeyDown(Key::A)` → 位置に `speed * dt` を足す |
| ぶつかったか調べる | `MakeOBB` で箱を作る → `Intersects(a, b)` |
| マウスで 3D の物を選ぶ | `camera.ScreenPointToRay(m.x, m.y, 幅, 高さ)` → `Raycast(ray, box)` |
| 当たり判定の箱を、目で見る | `DrawWireBox(box, 色)`(`OnRender` で) |
| 画像を貼る | `CreateTexture(path)` → `DrawMesh(mesh, 行列, texture)` |
| 敵を、ノードで動かす | `KRBehaviorEditor` でグラフを作る → `LoadGraph` + `Runner` |
| 効果音・BGM | `GetAudio().Load` / `Play` |
| 値を調整しながら試す | `OnImGui` に、スライダー(`ImGui::SliderFloat`) |
| 値が出ない・動かないとき | `KR_LOG_INFO` で、値をログに出す(Debug のコンソール) |

## 6. よくある間違い
| 症状 | 原因と直し方 |
|---|---|
| 何も描かれない | `CreateMesh` の戻り値の `IsValid()` を確認。`OnRender` の中で `DrawMesh` を呼んでいるか。カメラが、物から遠すぎ・向きが違う |
| 描かれたが、真っ暗・真っ白 | `Lighting` の `toLight`・`ambient` を確認 |
| 物が、鏡のように左右逆 | 左手系。+X が右・+Z が奥 |
| 動きの速さが、PC ごとに違う | `deltaTime` を掛けていない |
| キーが反応しない | ImGui の窓の上にカーソルがある(窓を使っている間は、ゲームに届かない) |
| `GetAssetDirectory` の先に、ファイルが無い | ゲームの CMake で `KR_GAME_ASSETS_DIR` を決めて、`kr_copy_runtime_files` を呼んでいるか([Usage.md](Usage.md) の 2-2)。`Enemies` フォルダーを**初めて作った**ときは、CMake を、もう一度 configure |
| 日本語を含むフォルダーで落ちる | ファイルのパスは `std::filesystem::path`。文字列と `path` を行き来するときは、`u8string()` / `char8_t` |
| Release で、ログが出ない | `KR_LOG_TRACE` / `KR_LOG_INFO` は、Release で消える(`WARN` 以上は出る) |
| `External/KREngine が、空です` | サブモジュールを取っていない。`git submodule update --init`(すれすれ飛行は、`Setup.ps1`) |

## 7. 次に読む資料
- **関数の詳しい説明(機能ごとのページ。サンプルコード・引数・戻り値)**:[Reference/index.md](Reference/index.md)
- **この文書の次**:[Usage.md](Usage.md)(CMake の書き方・Debug と Release・エンジンをゲームと一緒に開発する・モデル・敵の行動・音の詳しい手順)
- **全体の目次**:[Structure.md](Structure.md)
- **なぜ、こう作ったか**:[History.md](History.md)(経緯・決めたこと・選ばなかった案)
- **ツール**:[AnimationEditor.md](AnimationEditor.md)(判定の形の調整)・[BehaviorEditor.md](BehaviorEditor.md)(敵の行動のグラフ)
- **見本**:`samples/Minimal`(最小)・`samples/Animated`(動くモデル)・`samples/Behavior`(敵の行動)・`samples/Audio`(音)
