#pragma once

#include "Graphics/DX12/DX12Common.h"

#include <memory>
#include <span>

namespace TenFiftyEight
{
    struct DX12PipelineDesc
    {
        D3D12_SHADER_BYTECODE vertexShader = {};
        D3D12_SHADER_BYTECODE pixelShader = {};
        std::span<const D3D12_INPUT_ELEMENT_DESC> inputLayout; // 頂点データの形式
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
