# KR エンジンの構成(フォルダ・クラス・ビルド)

KR エンジンの、どのフォルダに何があり、クラスがどう関係し、ビルドで何が行われるか。まず、使い方は [Usage.md](Usage.md)。

## 資料の案内
| 知りたいこと | ファイル |
|---|---|
| **使い方(最短の入門)** | [Usage.md](Usage.md) |
| 起動から終了までの流れ・初期化・メインループ | [Lifecycle.md](Lifecycle.md) |
| ウィンドウ・OS メッセージ・ログ | [Platform.md](Platform.md) |
| 描画の仕組み(DX12 の部品・描画の流れ) | [Rendering.md](Rendering.md) |
| 形(メッシュ)・OBJ の読み込み | [Mesh.md](Mesh.md) |
| 画像(テクスチャ) | [Textures.md](Textures.md) |
| 光・フォグ・空・共通のシェーダー | [Lighting.md](Lighting.md) |
| 入力(キーボード) | [Input.md](Input.md) |
| 当たり判定(OBB・線の描画) | [Collision.md](Collision.md) |
| デバッグ用の画面(ImGui) | [ImGui.md](ImGui.md) |
| 調べ物:ボーンアニメーション | [Animation.md](Animation.md) |
| 調べ物:演出(爆発・シェーダーなど) | [Effects.md](Effects.md) |

---

## フォルダ構成

```
KREngine/
├─ include/KR/                   【公開】ゲームから見える
│   ├─ KR.h                        まとめヘッダー(主なヘッダーを、まとめて取り込む)
│   ├─ Core/                       Application, Log, Input(IsKeyDown / IsKeyPressed / IsKeyReleased), Assets(アセット・シェーダーの場所)
│   ├─ Physics/                    Collision(OBB・MakeOBB・Intersects・GetCorners。当たり判定)
│   ├─ Animation/                  Skeleton(骨の階層)・AnimationClip(動き)・GltfLoader(LoadGltf。glTF を読む)
│   └─ Renderer/                   Renderer, Camera, Mesh, Texture(TextureHandle), ModelLoader(LoadObj), Lighting(Lighting / Sky / Fog の設定)
├─ src/                          【非公開】ゲームからは見えない
│   ├─ Core/                       Application, EntryPoint(main), Log, Input(状態表), InputState(書き込み口), Assets
│   ├─ Platform/Windows/           Window(Win32)
│   ├─ Physics/                    Collision(判定の計算)
│   ├─ Animation/                  GltfLoader(cgltf で glTF を読み、エンジンのデータにする)
│   ├─ Renderer/                   Renderer(描画の統括), Camera, Mesh(キューブなどのデータ), ModelLoader(OBJ を読む), Image(PNG をほどく。stb_image)
│   ├─ UI/                         ImGuiLayer(ImGui の準備・片付け・毎フレームの描画)
│   └─ Graphics/DX12/              DX12 の部品
│       ├─ DX12Common.h              ComPtr, KR_DX_CHECK, 定数
│       ├─ DX12Device                GPU の選択, Device
│       ├─ DX12CommandQueue          Queue + Fence
│       ├─ DX12SwapChain             バックバッファ 2枚, RTV(sRGB)
│       ├─ DX12DepthBuffer           深度バッファ(D32_FLOAT), DSV
│       ├─ DX12DescriptorHeap        シェーダーから見える札掛け(SRV ヒープ)と枠の貸し借り
│       ├─ DX12CommandList           Allocator 2つ + Command List
│       ├─ DX12ShaderCompiler        HLSL → GPU の機械語(DXC)
│       ├─ DX12Pipeline              Root Signature(b0, b1, t0 + 静的サンプラー s0)+ PSO
│       ├─ DX12Buffer                Upload ヒープのバッファ(頂点・番号・定数)
│       └─ DX12Texture               画像(Default ヒープ + SRV の札。Upload 経由でコピー)
├─ shaders/                      シェーダー(実行時にコンパイル): Basic.hlsl(物)/ Sky.hlsl(空)/ Line.hlsl(線。当たり判定の箱)/ Common.hlsli(共通の b1 の宣言)
├─ samples/Minimal/              最小のサンプル(ウィンドウ + 回るキューブ + ImGui の FPS)。新しいゲームの雛形。assets は samples/Minimal/Assets/(テスト用のモデル)
├─ docs/                         この資料
├─ CMakeLists.txt                エンジンのビルド(ライブラリ KREngine・関数 kr_copy_runtime_files・サンプル)
└─ CMakePresets.json             エンジンを単体でビルドするときの設定(vs2026)
```

---

## クラスの関係

```mermaid
classDiagram
    direction LR

    class Application {
        <<公開>>
        +Run()
        #OnInit() / OnUpdate(dt) / OnRender() / OnImGui() / OnShutdown()
        #GetRenderer() Renderer
    }
    class Input {
        <<公開>>
        +IsKeyDown(Key)
        +IsKeyPressed(Key)
        +IsKeyReleased(Key)
    }
    class InputState {
        <<非公開>>
        OnKeyDown / OnKeyUp
        ReleaseAll / EndFrame
    }
    class Renderer {
        <<公開>>
        +SetClearColor(r, g, b)
        +SetVSync(on)
        +SetCamera(camera) / SetLighting / SetSky / SetFog
        +CreateMesh(data) MeshHandle
        +DestroyMesh(handle)
        +CreateTexture(file) TextureHandle
        +DrawMesh(handle, world, texture)
        +DrawLine / DrawWireBox
        -BeginFrame() / EndFrame()
        -ExecuteDrawList()
        -ReleaseFinishedMeshes()
    }
    class Camera {
        <<公開>>
        +SetPosition() / LookAt()
        +SetFieldOfView(度) / SetRollDegrees / SetClipPlanes()
        +GetViewMatrix()
        +GetProjectionMatrix(縦横比)
    }
    class MeshData {
        <<公開>>
        vertices(Vertex の配列)
        indices(uint32_t の配列)
    }
    class ModelLoader {
        <<公開>>
        +LoadObj(相対パス) MeshData
    }
    class MeshHandle {
        <<公開>>
        id(番号札。0 は無効)
    }
    class Window {
        <<非公開>>
        +ProcessMessages()
        -ToggleBorderless()
    }
    class ImGuiLayer {
        <<非公開>>
        +BeginFrame() / Render()
        +WantsKeyboard()
        +HandleMessage()
    }
    class DX12部品 {
        <<非公開 / DX12>>
        Device・Queue・SwapChain
        DepthBuffer・CommandList
        ShaderCompiler・Pipeline
        DescriptorHeap(SRV)
        UploadBuffer
    }

    Application *-- Window : 所有
    Application *-- Renderer : 所有
    Renderer *-- DX12部品 : 所有
    Renderer *-- ImGuiLayer : 所有
    Application ..> Renderer : Window のメッセージを ImGui へ渡す(MessageHook)
    Renderer o-- Camera : コピーを保持
    Application ..> InputState : Window の通知を書き込む / フレームの最後に EndFrame
    InputState ..> Input : 状態表(読む側は Input)
    ModelLoader ..> MeshData : 作って返す
```

ゲームは、`Application` を継承して、`Renderer` に、`SetCamera` / `CreateMesh` / `DrawMesh` などを頼み、`Input` を読む。

**依存のルール**:ゲームが使えるのは公開側(`include/`)の Renderer・Camera・Mesh・ModelLoader・Input などだけ。入力の状態表への書き込み口(`InputState`)は `src/` にあり、ゲームからは見えない(読むだけ)。DX12 の部品は、ゲームからは見えない。ゲームは、形の実体(GPU のバッファ)に触らず、**番号札(MeshHandle)**だけを持つ。

---

## ビルド時に行われること

```mermaid
flowchart LR
    B["DirectX-Headers と Dear ImGui(docking 版)<br/>(zip を自動ダウンロード)"] --> A["KREngine.lib"]
    S["stb_image(エンジンだけが使う)<br/>(自動ダウンロード)"] --> A
    A --> C["ゲームの exe / サンプル KRMinimal"]
    C --> D["ビルド後のコピー(kr_copy_runtime_files)<br/>dxcompiler.dll / dxil.dll<br/>エンジンの shaders/ → Shaders/<br/>ゲームのアセット(KR_GAME_ASSETS_DIR の下の全部のフォルダー)→ exe の横"]
```
