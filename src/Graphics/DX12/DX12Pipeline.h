#pragma once

#include "Graphics/DX12/DX12Common.h"

#include <memory>
#include <span>

namespace KR
{
    // Root Signature の何番目の項目か(SetGraphicsRoot... 系の関数の第1引数に使う)
    constexpr UINT ObjectConstantsRootIndex = 0; // b0: 物体ごとのデータ(ワールド行列)を直接入れる
    constexpr UINT FrameConstantsRootIndex = 1;  // b1: フレームごとのデータ(定数バッファ)の住所を入れる
    constexpr UINT TextureRootIndex = 2;         // t0: 画像の札(SRV)を指す口(descriptor table)。ピクセルシェーダーだけが使う
    constexpr UINT PaletteRootIndex = 3;         // b2: 骨の行列の表(パレット)の住所。スキニングの頂点シェーダーだけが使う(骨なしの物は、設定しない)

    struct DX12PipelineDesc
    {
        D3D12_SHADER_BYTECODE vertexShader = {};
        D3D12_SHADER_BYTECODE pixelShader = {};
        std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout; // 頂点データの形式(空なら、頂点バッファなし)

        // 指定したら、Root Signature を新しく作らず、これを使う
        // (命令リストに設定した Root Signature と、PSO の Root Signature は、同じ物である必要があるため。空と物で共有する)
        ID3D12RootSignature* sharedRootSignature = nullptr;

        bool depthEnabled = true;  // false: 深度テスト・深度書き込みをしない(空など、最初に描いて、あとから上書きされるもの)
        bool cullBackFaces = true; // false: 裏向きの三角形も描く(画面全体を覆う三角形の向きを、気にしなくて済む)
        bool lines = false;        // true: 頂点を 2 つずつ、線として組み立てる(デバッグの線)。false: 3 つずつ、三角形として組み立てる
    };

    // 描画の設定一式
    //   Root Signature : シェーダーにどんなデータを渡すかの宣言
    //   PSO            : シェーダー・頂点の形式・カリングなどの設定をまとめて固めたもの
    class DX12Pipeline
    {
    public:
        // ホットリロードでの失敗に備え、失敗時は停止せずに nullptr を返す
        static std::unique_ptr<DX12Pipeline> Create(ID3D12Device* device, const DX12PipelineDesc& desc);

        ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
        ID3D12PipelineState* Get() const { return m_PipelineState.Get(); }

    private:
        DX12Pipeline() = default;

        ComPtr<ID3D12RootSignature> m_RootSignature;
        ComPtr<ID3D12PipelineState> m_PipelineState;
    };
}
