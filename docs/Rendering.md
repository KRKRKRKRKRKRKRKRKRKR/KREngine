# 描画(座標変換・GPU・シェーダー)

座標がどう変換されて画面に出るか、GPU の描画パイプライン、C++ とシェーダーのつながり。

[← 資料の案内(Structure.md)に戻る](Structure.md)

## 座標変換の全体像

```mermaid
flowchart LR
    L["ローカル座標<br/>物体の中心が原点<br/>(キューブの角 0.5, 0.5, 0.5)"] -- "ワールド行列<br/>S → R → T<br/>Game が DrawMesh に渡す / b0" --> W["ワールド座標<br/>世界の原点が基準<br/>(1 = 1メートル)"]
    W -- "ビュー行列<br/>カメラの位置と向き" --> V["ビュー座標<br/>カメラが原点<br/>正面が +Z"]
    V -- "プロジェクション行列<br/>視野角 60度・縦横比<br/>near 0.1 / far 1000" --> P["画面の座標<br/>GPU が w で割る<br/>= 遠い物ほど小さい"]
```

- ビュー行列 × プロジェクション行列は、CPU で1つにまとめて **b1(定数バッファ)**で渡す([Renderer.cpp:211](../src/Renderer/Renderer.cpp#L211))
- 行列の作り方:[Camera.cpp](../src/Renderer/Camera.cpp)
- シェーダーでの掛け算:[Basic.hlsl:29](../shaders/Basic.hlsl#L29)


---

## GPU の中の流れ(描画パイプライン)

```mermaid
flowchart LR
    IA["① 頂点を集める<br/>番号リストの順に<br/>頂点リストから取り出す"] --> VS["② 頂点シェーダー<br/>頂点ごとに1回<br/>ワールド → カメラ → 画面"]
    VS --> RS["③ ラスタライザー<br/>裏向きの三角形を捨てる<br/>画素に分解・色を補間"]
    RS --> PS["④ ピクセルシェーダー<br/>画素ごとに1回<br/>色を決める"]
    PS --> DT["⑤ 深度テスト<br/>記録より手前なら合格<br/>記録を上書き"]
    DT --> OM["⑥ 書き込み<br/>sRGB に変換して<br/>バックバッファへ"]
```

| 段階 | 設定している場所 |
|---|---|
| ② ④ シェーダー | [Basic.hlsl](../shaders/Basic.hlsl)(物)/ [Sky.hlsl](../shaders/Sky.hlsl)(空) |
| ③ カリング(時計回りが表) | [DX12Pipeline.cpp](../src/Graphics/DX12/DX12Pipeline.cpp)(RasterizerState) |
| ⑤ 深度テスト(LESS・書き込みあり) | [DX12Pipeline.cpp](../src/Graphics/DX12/DX12Pipeline.cpp)(DepthStencilState) |
| ⑤ 深度バッファ | [DX12DepthBuffer.cpp](../src/Graphics/DX12/DX12DepthBuffer.cpp) |


---

## C++ とシェーダーのつながり

```mermaid
flowchart LR
    subgraph CPP["C++"]
        V["Vertex(Mesh.h)<br/>position XMFLOAT3<br/>normal XMFLOAT3<br/>color XMFLOAT4<br/>uv XMFLOAT2(48 バイト)"]
        IL["VertexInputLayout<br/>0 バイト目 = POSITION<br/>12 バイト目 = NORMAL<br/>24 バイト目 = COLOR<br/>40 バイト目 = TEXCOORD"]
        RC["b0: SetGraphicsRoot32BitConstants<br/>ワールド行列(物体ごと)"]
        CBV["b1: SetGraphicsRootConstantBufferView<br/>FrameConstants(フレームごと。Renderer.cpp)<br/>カメラ・光・フォグ・空"]
    end
    subgraph RSIG["Root Signature(物と空で共有)"]
        P0["0 番 = b0 に数字 16 個<br/>頂点シェーダーだけ"]
        P1["1 番 = b1 の住所<br/>頂点・ピクセルの両方(ALL)"]
        P2["2 番 = t0 に画像の札(SRV)<br/>ピクセルシェーダーだけ<br/>+ 静的サンプラー s0"]
    end
    subgraph HLSL["シェーダー"]
        VI["Basic.hlsl: VSInput<br/>: POSITION / : NORMAL / : COLOR / : TEXCOORD"]
        TX["Basic.hlsl: Texture2D t0 / SamplerState s0<br/>画像を Sample(uv)"]
        CB0["Basic.hlsl: cbuffer ObjectConstants b0<br/>world"]
        CB1["Common.hlsli: cbuffer FrameConstants b1<br/>(Basic.hlsl と Sky.hlsl が共有)"]
    end
    V --- IL
    IL -- 名前でつながる --> VI
    RC --> P0 -- 番号 b0 --> CB0
    CBV --> P1 -- 番号 b1 --> CB1
    TXC["t0: SetGraphicsRootDescriptorTable<br/>画像の札(画像が変わるときだけ)"] --> P2 -- 番号 t0 --> TX
```

| つながり | C++ 側 | シェーダー側 |
|---|---|---|
| 頂点データ | [VertexInputLayout(Renderer.cpp)](../src/Renderer/Renderer.cpp) | [VSInput(Basic.hlsl)](../shaders/Basic.hlsl) |
| b0 / b1 / t0 / s0 の宣言 | [DX12Pipeline.cpp(`CreateRootSignature`)](../src/Graphics/DX12/DX12Pipeline.cpp) | b0:[Basic.hlsl](../shaders/Basic.hlsl)、b1:[Common.hlsli](../shaders/Common.hlsli) |
| b2(骨の行列の表。パレット) | 同上(`PaletteRootIndex = 3`。骨つきの物を描くときだけ設定) | `cbuffer SkinPalette`([Basic.hlsl](../shaders/Basic.hlsl)。`VSSkinned` だけが読む) |
| b1 の中身(別紙) | `FrameConstants`([Renderer.cpp](../src/Renderer/Renderer.cpp)。`float4` にそろえる) | `cbuffer FrameConstants`([Common.hlsli](../shaders/Common.hlsli)。並びと大きさは C++ と同じ) |

**食い違いを防ぐ工夫**:b1 の宣言は、共通のファイル(`Common.hlsli`)に **1 か所だけ**書いて、Basic.hlsl と Sky.hlsl の両方が `#include` する。C++ の `FrameConstants` と HLSL の `FrameConstants` は、並びと大きさを同じにする。

---

## 物の描画と、空の描画(2 つのパイプライン)

| | 物(地面・障害物・自機など) | 空 |
|---|---|---|
| シェーダー | [Basic.hlsl](../shaders/Basic.hlsl) | [Sky.hlsl](../shaders/Sky.hlsl) |
| 頂点データ | 頂点バッファ・番号バッファ | **なし**(頂点シェーダーが、頂点の番号から、画面全体を覆う三角形を作る) |
| 深度テスト・書き込み | あり | **なし**(深度バッファを「一番奥」のままにして、あとから描く物が、空の上に上書きされる) |
| カリング | 裏向きを捨てる | なし |
| 描く順番 | `ExecuteDrawList`(EndFrame) | `BeginFrame` の最後(**最初**) |
| Root Signature | 共有(設定は `BeginFrame` で 1 回) | 同じ物を共有 |

- 陰影・フォグ・空の計算は、[Lighting.md](Lighting.md) にまとめてある。
