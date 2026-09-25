#include "Graphics/DX12/DX12Pipeline.h"

namespace TenFiftyEight
{
    std::unique_ptr<DX12Pipeline> DX12Pipeline::Create(ID3D12Device* device, const DX12PipelineDesc& desc)
    {
        std::unique_ptr<DX12Pipeline> pipeline(new DX12Pipeline());

        // Root Signature: 今はシェーダーに渡すデータがないので空。頂点バッファを使う宣言だけする
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        // 構造体 → バイナリに変換(シリアライズ)してから作成する決まりになっている
        ComPtr<ID3DBlob> serialized;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error)))
        {
            TFE_LOG_ERROR("Root Signature のシリアライズ失敗: {}",
                          error ? static_cast<const char*>(error->GetBufferPointer()) : "");
            return nullptr;
        }
        if (FAILED(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                               IID_PPV_ARGS(&pipeline->m_RootSignature))))
        {
            TFE_LOG_ERROR("Root Signature の作成失敗");
            return nullptr;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = pipeline->m_RootSignature.Get();
        psoDesc.VS = desc.vertexShader;
        psoDesc.PS = desc.pixelShader;
        psoDesc.InputLayout = { desc.inputLayout.data(), static_cast<UINT>(desc.inputLayout.size()) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // 裏向きの三角形は描かない(時計回りが表)
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);           // 半透明なし(上書き)
        psoDesc.DepthStencilState.DepthEnable = FALSE;                    // 深度バッファは 2-5 で追加する
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = RenderTargetFormat;
        psoDesc.SampleDesc.Count = 1;

        if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->m_PipelineState))))
        {
            TFE_LOG_ERROR("PSO の作成失敗(シェーダーの入力と頂点データの形式が合っているか確認)");
            return nullptr;
        }

        return pipeline;
    }
}
