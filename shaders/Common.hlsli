// 複数のシェーダー(Basic.hlsl・Sky.hlsl など)が共有する宣言。#include "Common.hlsli" で取り込む
// 「C++ 側の FrameConstants(Renderer.cpp)と対応する宣言」を、1か所だけに書くためのファイル
// (別々のファイルに書くと、片方だけ直し忘れて食い違い、読む値がずれて絵が狂う)

// b1 の別紙(定数バッファ)から受け取るデータ(C++ 側は Renderer::BeginFrame で1フレームに1回書く)
// 頂点シェーダーもピクセルシェーダーも読む(Root Signature の b1 は ALL)
// 並びと大きさは、C++ 側の FrameConstants と完全に同じにする
cbuffer FrameConstants : register(b1)
{
    row_major float4x4 viewProjection; // ビュー行列 × プロジェクション行列(カメラ)
    float4 toLight;                    // xyz = 物から太陽へ向かう向き(長さ 1。平行な光なので、世界のどこでも同じ)
    float4 lightColor;                 // rgb = 太陽の光の色(強さ込み)
    float4 ambientColor;               // rgb = 環境光(光が当たらない面の、最低限の明るさ)
    float4 cameraPosition;             // xyz = カメラの世界での位置(フォグの距離の計算に使う)
    float4 fogColor;                   // rgb = 霧の色(空の地平線の色と同じ。遠くが空に溶ける)
    float4 fogParams;                  // x = フォグの濃さ(0 ならフォグなし)
    row_major float4x4 inverseViewProjection; // viewProjection の逆(空: 画面の座標 → カメラからの向き。カメラの位置は含まない)
    float4 skyHorizon;                 // rgb = 地平線の色
    float4 skyZenith;                  // rgb = 天頂の色 / w = 地平線の光の幅
    float4 skyNadir;                   // rgb = 下の色(視線が地平線より下のとき)
};
