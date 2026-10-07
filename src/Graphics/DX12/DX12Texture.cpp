#include "Graphics/DX12/DX12Texture.h"

#include "Graphics/DX12/DX12Buffer.h"
#include "Graphics/DX12/DX12CommandList.h"
#include "Graphics/DX12/DX12CommandQueue.h"
#include "Graphics/DX12/DX12DescriptorHeap.h"

namespace KR
{
    namespace
    {
        // 画像の形式。_SRGB: 画像の色は、モニター用の値(ガンマ補正済み)で入っている。GPU が、読むときに、光の計算用の値(線形)に直す
        constexpr DXGI_FORMAT TextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    std::unique_ptr<DX12Texture> DX12Texture::Create(ID3D12Device* device, DX12CommandList& copyList, DX12CommandQueue& queue,
                                                     DX12DescriptorHeap& srvHeap, uint32_t width, uint32_t height, const uint8_t* rgbaPixels)
    {
        if (width == 0 || height == 0 || !rgbaPixels)
        {
            TFE_LOG_ERROR("DX12Texture: 画像の大きさが 0、または、ピクセルがありません");
            return nullptr;
        }

        std::unique_ptr<DX12Texture> texture(new DX12Texture());

        // ① 画像の置き場所(Default ヒープ)。最初は「コピーを受け取る」状態
        const CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
        const CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(TextureFormat, width, height, 1, 1);
        TFE_DX_CHECK(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                     D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture->m_Resource)));

        // ② 「Upload ヒープに、どう並べれば、コピーできるか」を聞く
        //    1 行の長さ(RowPitch)は、256 バイトの倍数にそろえる決まり。画像の 1 行(width × 4 バイト)が、半端なら、すき間があく
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        UINT rowCount = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 totalBytes = 0;
        device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &rowCount, &rowSizeInBytes, &totalBytes);

        // ③ Upload ヒープ(荷物受け取り所)に、ピクセルを、1 行ずつ、並べる
        DX12UploadBuffer uploadBuffer(device, totalBytes);
        const size_t sourceRowBytes = static_cast<size_t>(width) * 4;
        for (UINT row = 0; row < rowCount; ++row)
        {
            uploadBuffer.Write(rgbaPixels + row * sourceRowBytes, sourceRowBytes, footprint.Offset + static_cast<UINT64>(row) * footprint.Footprint.RowPitch);
        }

        // ④ GPU に、「Upload の荷物を、Default の倉庫へ、コピーして」と頼む
        ID3D12GraphicsCommandList* commandList = copyList.Begin(0);
        const CD3DX12_TEXTURE_COPY_LOCATION destination(texture->m_Resource.Get(), 0);
        const CD3DX12_TEXTURE_COPY_LOCATION source(uploadBuffer.GetResource(), footprint);
        commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

        // ⑤ バリア: 「コピー先(書き込み中)」から「シェーダーが読む」に、状態を切り替える
        //    GPU に、「ここまでの書き込みが、終わってから、読み始めて」と伝える信号(書きかけの画像を、読まないため)
        const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            texture->m_Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);

        copyList.End();
        queue.Execute(commandList);
        queue.Flush(); // コピーが終わるまで待つ。終われば、Upload ヒープは、もう要らない(この関数を出るときに、捨てられる)

        // ⑥ シェーダーが、この画像を読むための札(SRV)を、札掛けの枠に掛ける
        if (!srvHeap.Alloc(&texture->m_SrvCpu, &texture->m_SrvGpu))
        {
            return nullptr; // 枠が無い(エラーは Alloc が出す)
        }
        texture->m_SrvHeap = &srvHeap;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = TextureFormat;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // R, G, B, A を、そのままの順で読む
        srvDesc.Texture2D.MipLevels = 1;                                            // ミップマップは、まだ無い(1 段だけ)
        device->CreateShaderResourceView(texture->m_Resource.Get(), &srvDesc, texture->m_SrvCpu);

        return texture;
    }

    DX12Texture::~DX12Texture()
    {
        if (m_SrvHeap)
        {
            m_SrvHeap->Free(m_SrvCpu);
        }
    }
}
