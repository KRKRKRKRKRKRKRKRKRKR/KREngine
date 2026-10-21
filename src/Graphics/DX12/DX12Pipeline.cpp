#include "Graphics/DX12/DX12Pipeline.h"

#include "Graphics/DX12/DX12DepthBuffer.h"

namespace KR
{
    namespace
    {
        // Root Signature(シェーダーへの受け渡し棚の設計図)を作る。失敗したら false
        //   0番: b0 の箱に、4バイトの数字を16個(= 4x4 のワールド行列)。頂点シェーダーだけが使う
        //   1番: b1 の箱に、定数バッファ(別紙)の住所。頂点シェーダーもピクセルシェーダーも使う(ALL)
        //        (カメラの行列は頂点シェーダー、光やフォグはピクセルシェーダーが使うため)
        //   2番: t0 の口に、画像の札(SRV)。札掛け(SRV ヒープ)の中の、どの札を指すかは、描くたびに変えられる。ピクセルシェーダーだけが使う
        //   3番: b2 の箱に、定数バッファ(骨の行列の表 = パレット)の住所。頂点シェーダーだけが使う(骨つきの物を描くときだけ設定する)
        //   サンプラー(s0):画像から色を取るときの読み方。設計図に埋め込む(静的サンプラー)。線形(隣のピクセルと混ぜる)・繰り返し
        bool CreateRootSignature(ID3D12Device* device, ComPtr<ID3D12RootSignature>& outRootSignature)
        {
            CD3DX12_DESCRIPTOR_RANGE textureRange;
            textureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 から 1 個(SRV)

            CD3DX12_ROOT_PARAMETER rootParameters[5];
            rootParameters[ObjectConstantsRootIndex].InitAsConstants(16, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
            rootParameters[FrameConstantsRootIndex].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);
            rootParameters[TextureRootIndex].InitAsDescriptorTable(1, &textureRange, D3D12_SHADER_VISIBILITY_PIXEL);
            rootParameters[PaletteRootIndex].InitAsConstantBufferView(2, 0, D3D12_SHADER_VISIBILITY_VERTEX);
            rootParameters[MaterialRootIndex].InitAsConstants(MaterialConstantsCount, 3, 0, D3D12_SHADER_VISIBILITY_PIXEL);

            const CD3DX12_STATIC_SAMPLER_DESC linearSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                                                            D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                                            D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0.0f, 1, D3D12_COMPARISON_FUNC_NEVER,
                                                            D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK, 0.0f, D3D12_FLOAT32_MAX,
                                                            D3D12_SHADER_VISIBILITY_PIXEL);

            CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &linearSampler,
                                   D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            // 構造体 → バイナリに変換(シリアライズ)してから作成する決まりになっている
            ComPtr<ID3DBlob> serialized;
            ComPtr<ID3DBlob> error;
            if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error)))
            {
                KR_LOG_ERROR("Root Signature のシリアライズ失敗: {}",
                              error ? static_cast<const char*>(error->GetBufferPointer()) : "");
                return false;
            }
            if (FAILED(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                                   IID_PPV_ARGS(&outRootSignature))))
            {
                KR_LOG_ERROR("Root Signature の作成失敗");
                return false;
            }
            return true;
        }
    }

    std::unique_ptr<DX12Pipeline> DX12Pipeline::Create(ID3D12Device* device, const DX12PipelineDesc& desc)
    {
        std::unique_ptr<DX12Pipeline> pipeline(new DX12Pipeline());

        if (desc.sharedRootSignature)
        {
            // 4-7b: すでにある Root Signature を共有する(新しく作らない)
            pipeline->m_RootSignature = desc.sharedRootSignature;
        }
        else if (!CreateRootSignature(device, pipeline->m_RootSignature))
        {
            return nullptr;
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = pipeline->m_RootSignature.Get();
        psoDesc.VS = desc.vertexShader;
        psoDesc.PS = desc.pixelShader;
        psoDesc.InputLayout = { desc.inputLayout.data(), static_cast<UINT>(desc.inputLayout.size()) };
        // 頂点を、何として組み立てるか(線なら 2 つずつ、三角形なら 3 つずつ)。描くときの IASetPrimitiveTopology と合わせる
        psoDesc.PrimitiveTopologyType = desc.lines ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // 裏向きの三角形は描かない(時計回りが表)
        if (!desc.cullBackFaces)
        {
            psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;      // 空: 向きに関係なく描く
        }
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);           // 半透明なし(上書き)
        // 深度テスト: 「これから塗る物の奥行き < 記録されている奥行き」なら塗り、記録を上書きする
        // 空(depthEnabled = false)は、テストも書き込みもしない → 深度バッファは「一番奥(1.0)」のままなので、
        // あとから描く地面や物は、必ず合格して、空の画素を上書きする
        psoDesc.DepthStencilState.DepthEnable = desc.depthEnabled ? TRUE : FALSE;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;    // Reversed-Z にするときは GREATER
        psoDesc.DepthStencilState.DepthWriteMask = desc.depthEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO; // 半透明の物では ZERO(記録しない)にする
        psoDesc.DSVFormat = DepthBufferFormat;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = RenderTargetFormat;
        psoDesc.SampleDesc.Count = 1;

        if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->m_PipelineState))))
        {
            KR_LOG_ERROR("PSO の作成失敗(シェーダーの入力と頂点データの形式が合っているか確認)");
            return nullptr;
        }

        return pipeline;
    }
}
