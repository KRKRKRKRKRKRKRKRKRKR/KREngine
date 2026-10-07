# テクスチャ(画像を貼る。フェーズ6)

モデルに、**画像(PNG)を貼る**仕組み(フェーズ6)。自機(`Player/player.obj`)に、`player.png` を貼るのが、最初の目標。

[← 資料の案内(Structure.md)に戻る](Structure.md)

**実装状況**:6-0(自機を Player モデルに差し替え)済み / 6-1(PNG を CPU に読む)済み / 6-2(UV)済み / 6-3(GPU のテクスチャ)済み / 6-4(画像を貼る)済み / 6-5(自機に適用)済み / 6-6(docs の整理)済み。**フェーズ6 完了**

## 決めたこと

| 項目 | 決定 |
|---|---|
| PNG の読み込み | **stb_image**(FetchContent。コミットを固定) |
| テクスチャの指定 | `DrawMesh(mesh, world, texture)`(3 つ目を省くと、白の 1×1 = 今までと同じ見た目) |
| 画像の形式 | `R8G8B8A8_UNORM_SRGB`(バックバッファが sRGB で、光を線形で計算しているため。GPU が、読むときに線形に直す) |
| ミップマップ | 今回は無し(小さく映ったときのちらつきが出たら、あとで足す) |
| サンプラー | 静的サンプラー(Root Signature に埋め込む。線形・繰り返し) |
| 画像の向き | OBJ の v は下が 0、D3D の v は上が 0 → 読むときに `1 − v` |

## 手順

| 手順 | 内容 | 画面の変化 |
|---|---|---|
| 6-0 | 自機を、猫のモデルから Player モデルに差し替え(画像はまだ無いので白) | 自機が、白い平たい円形になる |
| 6-1 | stb_image を取り込み、PNG を CPU に読む(`LoadImageRgba8`) | なし(ログで確認) |
| 6-2 | `Vertex` に `uv`(3 か所同時)。`LoadObj` が `vt` と `map_Kd` を読む | なし |
| 6-3 | テクスチャの GPU 側(`DX12Texture`・`CreateTexture` / `DestroyTexture`・白の 1×1) | なし |
| 6-4 | Root Signature に SRV のテーブルと静的サンプラー。`DrawMesh` の引数。シェーダーで `Sample` | 画像が貼れる |
| 6-5 | 自機に適用。向き・大きさ・高さの調整 | 自機に絵が付く |
| 6-6 | docs の仕上げ | — |

---

## 6-1. PNG を CPU に読む

**目的**:PNG ファイルを、**ピクセルの色の並び**(1 ピクセル 4 バイト:R・G・B・A)にほどく。GPU が読めるのは、圧縮されていない、色の並びだけだから。

たとえ話:**圧縮された絵のファイルは、「折りたたまれた地図」**。GPU は、「広げた地図」しか読めない。stb_image は、地図を広げる係。

```
player.png(約 70 KB。圧縮されている)
   │ LoadImageRgba8(パス)
   │   ① ファイルを、まるごと、メモリに読む(std::ifstream)
   │   ② stbi_load_from_memory で、ほどく(必ず 4 チャンネル = RGBA)
   │   ③ 自分の配列(ImageData)にコピーして、stb のメモリを返す
   ▼
ImageData { width = 1024, height = 1024, pixels = 1024 × 1024 × 4 バイト(約 4 MB)}
```

### どこに置くか、なぜそこか

| 処理 | 置く場所 | なぜそこか |
|---|---|---|
| stb の取得 | Engine/CMakeLists.txt | 画像を GPU に送るのは Engine の仕事。Game は、stb を知らない(`PRIVATE`) |
| `LoadImageRgba8` | Engine/src/Renderer/Image.h / .cpp(非公開) | Game は、`CreateTexture`(6-3)だけ使う。ほどく処理は、Engine の中の話 |
| ファイルを `std::ifstream` で読んでから渡す | Image.cpp | stb の `stbi_load` に、パスを渡すと、日本語などが入ったパスで、開けないことがあるため |
| 関数名が `LoadImageRgba8` | Image.h | Windows のヘッダーに、`LoadImage` というマクロがあり、ぶつかるため |
| `STB_IMAGE_IMPLEMENTATION` | Image.cpp だけ | stb は、ヘッダー 1 つ。関数の本体は、1 か所の `.cpp` だけで、取り込む |

### 動作確認

起動時のログに、`Image: player.png 1024x1024(元は 4 チャンネル)` が出る(6-1 では、Renderer のコンストラクタに、一時的な確認の呼び出しがある。6-3 で消す)。

---

## 6-2. 頂点に UV を足す(`LoadObj` が `vt` と `map_Kd` を読む)

**目的**:3D の形の、**どの場所に、画像の、どの部分を貼るか**を、頂点ごとに持たせる。まだ、画像は、貼らない(見た目は、変わらない)。

**UV とは**:画像の上の座標。U が横(左 0 〜 右 1)、V が縦(0 〜 1)。たとえ話:**サイコロの面に、シールを貼る**。面の 4 つの角に、「シールの、どこを、ここに合わせるか」を決めておく。**角と角の間は、GPU が自動で補間**して、絵を貼る(法線・色と同じ仕組み)。決めておかないと、貼る位置が、ずれる。

### 書いたこと

| ファイル | 内容 |
|---|---|
| [Mesh.h](../include/KR/Renderer/Mesh.h) | `Vertex` に `uv`(`XMFLOAT2`。初期値 (0, 0))。`MeshData` に `diffuseTexturePath`(貼る画像のフルパス) |
| [Renderer.cpp](../src/Renderer/Renderer.cpp) | 入力レイアウトに `TEXCOORD`(`R32G32_FLOAT`)を足した |
| [Basic.hlsl](../shaders/Basic.hlsl) | `VSInput` に `uv : TEXCOORD`。`PSInput` に `uv : TEXCOORD2`。頂点シェーダーで、通すだけ |
| [ModelLoader.cpp](../src/Renderer/ModelLoader.cpp) | ① `vt` を読む。**v は `1 − v`**(OBJ は下が 0、D3D は上が 0)② 面の `位置/UV/法線` の、UV の番号を読む(`ParseFaceElement`)③ 頂点の台帳のキーに、UV の番号を足す(UV が違えば、別の頂点)④ `.mtl` の `newmtl` を、白で登録(`Kd` が無く、`map_Kd` だけの材質も、「未知の材質」の警告にならない)⑤ `map_Kd` を読んで、`MeshData::diffuseTexturePath` に入れる(今は、形ごとに 1 枚だけ) |

**3 か所を同時に直す理由**:頂点データの形は、C++ の `Vertex`・入力レイアウト・シェーダーの `VSInput` の、3 か所に書いてある。**名前・順番・大きさが、1 つでも合わないと、PSO の作成に失敗する**(または、絵が壊れる)。

### 動作確認

| 確認すること | 期待する結果 |
|---|---|
| 見た目 | 6-1 までと、同じ(自機は、白) |
| ログ | `LoadObj: Player/player.obj 頂点 ○ 三角形 992 法線(…) UV 775` と、`画像: player.png`。「未知の材質」の警告が、消える |
| 頂点の数 | 6-1 と同じ 2381(増えない。下の補足) |
| 他の物(地面・障害物・木・キューブ) | 今までと、同じ見た目 |

**確認で分かったこと**:ログは `頂点 2381 三角形 992 法線(ファイル 652 / 計算 0) UV 775`。頂点の数は、**UV を足しても、2381 のまま**だった(増えると予想していたが、外れた)。理由:この形は、法線が**面ごと**に別々にある(`vn` が、面の数に近い 652 個)ので、「位置 + 法線」の組が、**すでに、面の角ごとに、別々**になっていた。UV は、その別々の頂点に、くっついて来るだけで、**新しく分ける必要が出なかった**。もし、法線が、頂点で共有されていたら(スムーズシェード)、UV が違う場所で、頂点が、分かれて、増える。

---

## 6-3. テクスチャを GPU に置く(`DX12Texture` / `CreateTexture` / `DestroyTexture`)

**目的**:読んだ画像(CPU のピクセル)を、GPU が、速く読めるメモリに置いて、シェーダーが、見つけられる札(SRV)を、掛ける。まだ、描画には使わない(6-4 から)。

### GPU のメモリは 2 種類

| 種類 | 役目 | たとえ |
|---|---|---|
| Upload ヒープ | CPU が書き込める。GPU も読める。**遅い** | 玄関の荷物受け取り所 |
| Default ヒープ | CPU は直接書けない。GPU だけ。**速い** | 倉庫の中 |

画像は、大きく(約 4 MB)、毎フレーム何度も読み、書き換えない → **Default ヒープ**。作るときだけ、Upload 経由でコピーする(頂点・番号は、小さいので、Upload に置きっぱなし)。

### 流れ(`DX12Texture::Create`)

```
① Default ヒープに、画像の置き場所を作る(最初は「コピーを受け取る」状態 = COPY_DEST)
② GetCopyableFootprints:Upload ヒープに、どう並べれば、コピーできるか(1 行の長さは、256 バイトの倍数)
③ Upload ヒープ(DX12UploadBuffer)に、ピクセルを、1 行ずつ並べる
④ CopyTextureRegion:Upload → Default のコピーを、GPU に頼む(m_UploadCommandList)
⑤ バリア:COPY_DEST → PIXEL_SHADER_RESOURCE(「書き込みが終わってから、読み始めて」の信号。書きかけを読まないため)
   → 命令を提出して、Flush(コピーが終わるまで待つ)。Upload ヒープは、ここで、捨てる
⑥ SRV(シェーダーが画像を読むための札)を、m_SrvHeap の枠に掛ける(ImGui のフォントと同じ棚)
```

### 窓口(Renderer)

| 関数 | 内容 |
|---|---|
| `CreateTexture(ファイル)` | `LoadImageRgba8` で読んで、`DX12Texture::Create`。番号札(`TextureHandle`)を返す。失敗したら無効な番号札 |
| `DestroyTexture(番号札)` | 番号札をすぐ無効にして、実体は「捨てる予定リスト」へ。GPU が使い終わってから(Fence)、本当に捨てる(SRV の枠も、このとき返る)。`DestroyMesh` と同じ |
| 白の 1×1(`m_WhiteTexture`) | Renderer が 1 つ持つ。画像を貼らない物に使う(白を掛けても、色が変わらない)。6-4 で使う |

### どこに置くか、なぜそこか

| 処理 | 置く場所 | なぜそこか |
|---|---|---|
| GPU のメモリへの置き方・SRV | Engine/src/Graphics/DX12/DX12Texture.h / .cpp(非公開) | DX12 の話。Game は、DX12 を知らない |
| 番号札・作る・捨てる | Renderer(`TextureHandle` は公開ヘッダー Texture.h) | Game は、番号札だけ持つ(形の番号札と同じ考え方) |
| コピー専用の命令リスト | Renderer の `m_UploadCommandList` | 毎フレームの命令リストとは別。コピーのたびに、GPU の完了を待つので、Allocator は 1 つでよい |
| `LoadObj` が返した画像のパスで、`CreateTexture` | Game(GameApp::OnInit) | どの画像を、どの形に貼るかは、ゲームが決める |

### 動作確認

| 確認すること | 期待する結果 |
|---|---|
| ログ | `Image: player.png 1024x1024` と、`Texture: player.png を GPU に置きました(番号札 2)`(白の 1×1 が、番号札 1) |
| 見た目 | 今までと同じ(まだ貼らない) |
| エラー | DX12 のデバッグレイヤーのエラーが出ない |
| 終了 | 「DX12 オブジェクトの解放漏れなし」 |

---

## 6-4. 画像を、シェーダーで読む(Root Signature・`DrawMesh`・`Sample`)

**目的**:画像を、実際に、貼る。シェーダーが、UV の位置の色を、画像から取って、描く。

たとえ話:画像は、倉庫の**写真集**。Root Signature の **t0** は、「写真集を置く、机の穴」(どの写真集を置くかは、描くたびに、変えられる)。**サンプラー(s0)**は、「虫眼鏡の使い方」(拡大したとき、なめらかに混ぜる=線形)。

### 書いたこと

| ファイル | 内容 |
|---|---|
| [DX12Pipeline.cpp](../src/Graphics/DX12/DX12Pipeline.cpp) / [.h](../src/Graphics/DX12/DX12Pipeline.h) | Root Signature に、**2 番:t0 の口**(descriptor table。SRV 1 個。ピクセルシェーダーだけ)と、**静的サンプラー s0**(線形・繰り返し)を足した。`TextureRootIndex = 2` |
| [Basic.hlsl](../shaders/Basic.hlsl) | `Texture2D albedoTexture : register(t0)` と `SamplerState linearSampler : register(s0)`。`albedo = Sample(linearSampler, uv)`。**色 = 頂点の色 × 画像の色 × 光** |
| [Renderer.h](../include/KR/Renderer/Renderer.h) / [.cpp](../src/Renderer/Renderer.cpp) | `DrawMesh(mesh, world, texture = {})`。描画リストに画像の番号札を持たせる。`BeginFrame`:`SetDescriptorHeaps`(SRV ヒープ)+ t0 に白を置く。`ExecuteDrawList`:画像が前と変わるときだけ、`SetGraphicsRootDescriptorTable`。無効な番号札は、白(エラーは 1 回だけ) |
| GameApp.cpp | 自機を、`DrawMesh(m_PlayerMesh, playerWorld, m_PlayerTexture)` で、画像つきで描く |

### 流れ

```
BeginFrame:  SetDescriptorHeaps(SRV ヒープ) → t0 に、白の 1×1 を置く → 空を描く(画像は、読まない)
ExecuteDrawList(物を、1 つずつ):
   画像が、前と変わるなら → SetGraphicsRootDescriptorTable(t0 に、この画像の札)
   → ワールド行列を b0 へ → DrawIndexedInstanced
ピクセルシェーダー:  色 = 頂点の色 × Sample(画像, uv) × 光 → フォグ
```

### なぜ、画像を貼らない物にも、白の 1×1 を使うのか

画像なしの物を、別のシェーダー(別のパイプライン)にすると、パイプラインが 2 つになる(切り替えの処理・2 つ分の保守が増える)。白の 1×1 を貼れば、`色 × 白 = 色` なので、**同じシェーダーのまま**、画像なしの物は、今までと同じ色になる。

### 動作確認

| 確認すること | 期待する結果 |
|---|---|
| 自機 | player.png の絵が、貼られている(向き・上下が正しい) |
| 他の物(地面・障害物・木・キューブ・空) | 今までと、同じ見た目 |
| エラー | DX12 のデバッグレイヤーのエラー・警告が出ない |
| 画像の色 | 暗すぎ・明るすぎにならない(sRGB の扱いが正しい) |

---

## 6-5. 自機に適用する

自機を、`Player/player.obj` + `player.png` にした(6-0 で形、6-4 で画像)。向き(`PlayerYaw = 0`)・大きさ(1.15 倍)・足もとの高さは、動かして確認して、そのままでよかった。`GameApp::OnInit` で、`LoadObj` が返した `diffuseTexturePath` を `CreateTexture` に渡し、`OnRender` で `DrawMesh(m_PlayerMesh, 行列, m_PlayerTexture)`、`OnShutdown` で `DestroyTexture`。

## 全体像(フェーズ6 が終わった時点)

```
player.obj / player.mtl(map_Kd player.png)/ player.png
  │ LoadObj:vt を読む(v は 1 − v)・map_Kd のパスを MeshData に入れる
  ▼
MeshData { 頂点(位置・法線・色・uv)・番号・diffuseTexturePath }
  │ CreateMesh                          │ CreateTexture(diffuseTexturePath)
  ▼                                     ▼  LoadImageRgba8(stb_image)→ Upload → コピー → バリア → SRV
形の番号札                               画像の番号札(Default ヒープ + SRV の枠)
  └──────── DrawMesh(形, 行列, 画像) ────────┘   画像を省くと、白の 1×1
                  │ ExecuteDrawList:画像が変わるときだけ、t0 に札を置く
                  ▼
ピクセルシェーダー:色 = 頂点の色 × Sample(画像, uv) × 光 → フォグ
```

## 既知の制限・これから

| 項目 | 内容 |
|---|---|
| 画像の枚数 | 形ごとに 1 枚だけ(材質ごとに別の画像は、まだ。`.mtl` の最初の `map_Kd` だけ使う) |
| ミップマップ | 無し。画像が、小さく映ったとき、ちらつくなら、足す |
| 画像の作成 | 起動時の読み込み用(`Flush` で待つので、走りながら作ると、止まる) |
| SRV の枠 | 64 枠のうち、ImGui・白・自機の画像で、使っている。画像が増えたら、`SrvHeapSize` を見る |
| 大文字・小文字 | `.obj` の `mtllib` と、ファイル名の、大文字・小文字がずれても、Windows は動く(Linux・Mac では、開けない) |
| 色の形式 | 画像は sRGB(`R8G8B8A8_UNORM_SRGB`)。法線マップなど、色でないデータは、別の形式(UNORM)が要る |
