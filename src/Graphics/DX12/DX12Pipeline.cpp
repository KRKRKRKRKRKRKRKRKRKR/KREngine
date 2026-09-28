#include "Graphics/DX12/DX12Pipeline.h"

#include "Graphics/DX12/DX12DepthBuffer.h"

namespace TenFiftyEight
{
    std::unique_ptr<DX12Pipeline> DX12Pipeline::Create(ID3D12Device* device, const DX12PipelineDesc& desc)
    {
        std::unique_ptr<DX12Pipeline> pipeline(new DX12Pipeline());

        // Root Signature(シェーダーへの受け渡し棚の設計図)
        //   0番: b0 の箱に、4バイトの数字を16個(= 4x4 のワールド行列)。頂点シェーダーだけが使う
        CD3DX12_ROOT_PARAMETER rootParameters[1];
        rootParameters[ObjectConstantsRootIndex].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr,
                               D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
        // 深度テスト: 「これから塗る物の奥行き < 記録されている奥行き」なら塗り、記録を上書きする
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;    // Reversed-Z にするときは GREATER
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 半透明の物では ZERO(記録しない)にする
        psoDesc.DSVFormat = DepthBufferFormat;
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
