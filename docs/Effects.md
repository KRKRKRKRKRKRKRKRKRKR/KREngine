---
title: 演出(エフェクト)の調べ物:種類・仕組み・このエンジンでの作り方
project: KREngine
kind: 設計
status: 最新
updated: 2026-10-07
replaces:
---

# 演出(エフェクト)の調べ物:種類・仕組み・このエンジンでの作り方

爆発・シールド・ブーストなどの演出を作るために、**どんな物があるか・どう作るか**を調べて、まとめた。実装は、これを見て、順番を決める。

[← 資料の案内(Structure.md)に戻る](Structure.md)

**調べた日**:2026-10-07。出典は、一番下。**調べたのは、一般的な作り方**(多くは Unity や OpenGL の資料。考え方は、DirectX 12 でも、同じ)。このエンジンへの当てはめは、コードを見て、確かめた。

---

## 1. 演出の種類(6 つのグループ)

| グループ | 例 | 仕組み(ひとことで) | このエンジンに足す物 |
|---|---|---|---|
| **A カメラ・時間** | 画面揺れ・ヒットストップ・スローモーション・視野(済) | カメラの位置・角度を、短い間、ずらす。時間の進み方を、止める・遅くする | **なし**(Game のコードだけ) |
| **B 形を動かす** | 破片が飛び散る・自機が消える | 形(メッシュ)を、**CPU で、位置・回転を、計算して**、`DrawMesh` で描く | **なし**(今の描き方のまま) |
| **C 半透明の形** | 爆発の閃光・衝撃波のリング・シールド・スピード線 | **半透明(ブレンド)**で、形を描く。光る物は、**加算ブレンド**(足し算なので、描く順番が、関係ない) | **半透明用のパイプライン** |
| **D パーティクル** | 火花・煙・ブーストの尾 | 小さな板(ビルボード)を、**たくさん**出して、1 つずつ、動かして、消す | 半透明 + 板を描く仕組み |
| **E 画面全体(ポストプロセス)** | ブーストの放射状のブラー・画面のふち暗く(ビネット)・色ずれ・ブルーム(光のにじみ) | 絵を、いったん**画像(オフスクリーン)**に描いて、**画面全体に、シェーダーをかけて**、画面に出す | **オフスクリーンに描く仕組み** |
| **F シェーダーの中の演出** | ディゾルブ(ノイズで、溶けて消える)・フレネル(ふちが光る)・流れる模様 | ピクセルシェーダーで、**時間・法線・ノイズ**を使って、色・透明度を決める | **時間を、シェーダーに渡す** |

## 2. 爆発の作り方(調べた結果)

爆発は、**いくつかの、小さな演出の組み合わせ**で、できている。

| 要素 | 内容 | 目安 |
|---|---|---|
| 閃光(フラッシュ) | 一瞬、**明るい黄白色**の球が、すぐ大きくなって、すぐ消える | 約 0.2 秒 |
| 衝撃波 | 輪が、広がりながら、薄くなる | |
| 火の玉 | 橙・赤の球。大きくなって、暗くなる | |
| 火花(エンバー) | 小さな粒が、飛び散る(尾を引く物もある) | |
| 破片 | 形のかけらが、飛び散って、落ちる | |
| 煙 | 残る。ゆっくり広がって、消える | |
| 物が消える | 爆発の瞬間に、元の物を消す | |
| 画面揺れ | **短く、小さく**(酔わないように)。衝撃波の速さに合わせて、**少し遅らせる**と、よい | |
| ヒットストップ | 重い一撃で、時間を、**50〜100 ミリ秒**止める。当たった重さが、出る | |

**画面揺れの作り方(トラウマ方式)**:「トラウマ」(0〜1)を、当たったときに上げ、時間で、減らす。揺れの強さ = トラウマの 2 乗〜3 乗。揺れの向きは、**乱数ではなく、ノイズ(パーリンノイズなど)**で決める → なめらかで、自然な揺れになる(乱数だと、ガタガタ、飛ぶ)。

## 3. 今のエンジンの現状(コードで確認したこと)

| 項目 | 今の状態 | 演出で、困る所 |
|---|---|---|
| パイプライン | `Basic`(形)・`Sky`(空)・`Line`(線)の 3 つ。Root Signature は共有 | 半透明用は、ない |
| ブレンド | `CD3DX12_BLEND_DESC(D3D12_DEFAULT)` = **半透明なし**(上書き)([DX12Pipeline.cpp](../src/Graphics/DX12/DX12Pipeline.cpp)) | 半透明・加算の演出が、できない |
| 深度の書き込み | `depthEnabled` で、テスト・書き込みを、一緒に切り替える | 半透明は、**テストは、する・書き込みは、しない**が必要(今は、分けられない) |
| 頂点シェーダー・ピクセルシェーダー | 光(Lambert)・フォグ・画像の色。**色の上書き(tint)・透明度・時間は、ない** | 色を変えて光らせる・フェードが、できない |
| 時間 | **シェーダーに、渡していない**(`FrameConstants` に、経過秒がない) | 流れる模様・ディゾルブが、できない |
| `DrawMesh` | `(形, 行列, 画像)` だけ。**色・透明度を、渡せない** | 球を、フェードさせられない |
| 動的な頂点 | `DrawLine` が、毎フレーム、Upload バッファに、頂点を書く仕組みを持つ | ビルボードの板にも、**同じ仕組みを、使い回せる** |
| 全画面の描画 | `Sky` が、**全画面を覆う三角形**を、描いている | ポストプロセスに、**同じ形を、使い回せる** |
| オフスクリーン(画像に描く) | **ない**(バックバッファに、直接描く) | ポストプロセス(E)は、ここから作る |
| compute シェーダー | ない | GPU パーティクル(数万個)は、まだ、いらない |
| ImGui | 画面の最後に、バックバッファへ描く | ポストプロセスの**後**に、描く必要がある |

## 4. 足す物の、難しさの順

| 順 | 足す物 | 難しさ | できるようになる演出 |
|---|---|---|---|
| 1 | 経過時間を、`FrameConstants` に足す | 易 | 流れる模様・ディゾルブ・光の点滅 |
| 2 | `DrawMesh` に、**色・透明度**を渡す(ルート定数に、色を足す) | 易 | 球のフェード・色つきの閃光 |
| 3 | **画面揺れ + ヒットストップ**(Game のコードだけ) | 易 | 当たった重さ(爆発の前に、効く) |
| 4 | **半透明のパイプライン**(アルファ・加算。深度の**書き込みだけ**切る)+ 描画リストを、「不透明 → 半透明」の順に | 中 | 閃光・衝撃波・シールド・スピード線 |
| 5 | **破片**(箱を、CPU で飛ばして、`DrawMesh`) | 中 | 爆発の破片 |
| 6 | **ビルボードの板を描く仕組み**(`DrawLine` の動的頂点を、まねる) | 中 | 火花・煙・ブーストの尾 |
| 7 | **フレネル**(ふちが光る)のシェーダー | 中 | シールドのアイテム |
| 8 | **ポストプロセスの土台**(オフスクリーン + 全画面三角形 + ImGui を、あとに) | 大 | ブーストの放射状ブラー・ビネット・色ずれ |
| 9 | **ブルーム**(明るい所を、取り出して、ぼかして、足す。HDR の画像) | 大 | 光のにじみ(爆発・ブーストが、映える) |
| 10 | GPU パーティクル(compute) | 大 | 数万個の粒(今は、いらない) |

## 5. おすすめの進め方

爆発(ゲームオーバーの演出)を、**目標**にして、必要な土台から、順に作る。

| 段階 | 内容 | 画面の変化 |
|---|---|---|
| E-1 | 土台:**経過時間**と、`DrawMesh` の**色・透明度** | 見た目は、変わらない |
| E-2 | **画面揺れ + ヒットストップ** | 当たった瞬間に、重さが出る |
| E-3 | **半透明のパイプライン**(アルファ・加算) | 半透明の球が、描ける |
| E-4 | **爆発**:自機が消える → 閃光の球 + 衝撃波のリング + 火の玉 + 破片 | ゲームオーバーで、爆発する |
| E-5 | **パーティクル**(ビルボードの板):火花・煙・ブーストの尾 | 爆発・ブーストが、賑やかになる |
| E-6 | **シールド**(フレネル + 加算)とアイテム | シールドが、光る |
| E-7 | **ポストプロセス**:ブーストの放射状ブラー・ビネット(→ ブルーム) | ブーストが、速く見える |

## 出典

- [Game Feel & Juice(画面揺れ・ヒットストップ・パーティクル・トゥイーン)](https://www.wayline.io/learn/game-feel/1)
- [Explosion effect help needed(閃光・衝撃波・破片・火の玉・煙の、構成)](https://realtimevfx.com/t/explosion-effect-help-needed/16021)
- [Make a Particle Explosion Effect(gamedev.net)](https://gamedev.net/tutorials/visual-arts/make-a-particle-explosion-effect-r2701)
- [Camera shake:トラウマ方式とノイズ(Squirrel Eiserloh の GDC 講演がもと)](https://dawnosaur.substack.com/p/spice-up-your-game-with-screen-shake)
- [Alpha Blending Part 2(Microsoft。半透明の、描く順番)](https://learn.microsoft.com/vi-vn/archive/blogs/etayrien/alpha-blending-part-2)
- [D3D12 の BLEND_OP(Xbox のドキュメント)](https://devdocs.xbox.com/reference/graphics/d3d12/enums/d3d12_blend_op_public)
- [GPU-based particle simulation(Wicked Engine)](https://wickedengine.net/2017/11/gpu-based-particle-simulation)
- [Energy shield(gamedev.net ShaderLab。フレネル・リム・波紋)](https://gamedev.net/shaderlab/35-energy-shield/)
- [Shield Shader with Intersection Highlight(Godot Shaders)](https://godotshaders.com/shader/shield-shader-with-intersection-highlight/)
- [How to implement a dissolve effect(ノイズのしきい値 + ふちの発光)](https://bugnet.io/blog/how-to-implement-a-dissolve-effect)
- [Anime Style Speedlines(放射状のスピード線)](https://godotshaders.com/?p=16778)
- [FidelityFX Lens(AMD。色ずれ・ビネット・フィルムグレイン。HLSL)](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/lens/)
- [DirectXTK の PostProcess(ブルームなど)](https://source-hub.etos-pro.ru/microsoft/DirectXTK/wiki/PostProcess)
- [HDR + Bloom(ダウンサンプル → ぼかし → 足す → トーンマップ)](https://daslang.io/doc/reference/tutorials/vulkan/11_hdr.html)
- [Compute Shader HDR and Bloom(Intel)](https://www.intel.com/content/www/cn/zh/developer/articles/code-sample/compute-shader-hdr-and-bloom.html)
